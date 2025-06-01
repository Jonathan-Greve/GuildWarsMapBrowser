#include "pch.h"
#include "byte_pattern_search_panel.h"
#include <filesystem>
#include <GuiGlobalConstants.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <algorithm>
#include <execution>
#include <vector>
#include <string>
#include <sstream>
#include <array>
#include <stdexcept>
#include <map>
#include <memory>
#include <unordered_set>
#include <cstdio>
#include <optional>
#include <cctype>

class BytePatternMatcher {
private:
	std::vector<std::optional<uint8_t>> pattern;
	std::array<size_t, 256> skip_table;

	void build_skip_table() {
		skip_table.fill(pattern.size());
		if (pattern.empty()) return;
		for (size_t i = 0; i < pattern.size() - 1; ++i) {
			if (pattern[i].has_value()) {
				skip_table[pattern[i].value()] = pattern.size() - 1 - i;
			}
		}
	}

public:
	BytePatternMatcher(const std::vector<std::optional<uint8_t>>& pat)
		: pattern(pat) {
		if (pattern.empty()) return;
		build_skip_table();
	}

	size_t get_pattern_size() const { return pattern.size(); }

	std::vector<size_t> search(const uint8_t* data, size_t data_size) const {
		if (pattern.empty() || data_size < pattern.size()) {
			return {};
		}

		std::vector<size_t> matches;
		if (pattern.size() > 0) {
			size_t estimated_matches = data_size / (pattern.size() * 50 + 100) + 1;
			matches.reserve(std::max(estimated_matches, static_cast<size_t>(16)));
		}

		const size_t pattern_len = pattern.size();
		const size_t last_pattern_idx = pattern_len - 1;
		size_t pos = 0;

		while (pos <= data_size - pattern_len) {
			int pattern_idx = static_cast<int>(last_pattern_idx);

			while (pattern_idx >= 0) {
				if (pattern[pattern_idx].has_value() && pattern[pattern_idx].value() != data[pos + pattern_idx]) {
					break;
				}
				--pattern_idx;
			}

			if (pattern_idx < 0) {
				matches.push_back(pos);
				pos += 1;
			}
			else {
				size_t char_in_text_at_last_pattern_pos = data[pos + last_pattern_idx];
				size_t skip = skip_table[char_in_text_at_last_pattern_pos];
				pos += std::max(skip, size_t(1));
			}
		}
		return matches;
	}
};

struct SearchResult {
	uint32_t file_id;
	uint32_t dat_alias;
	std::vector<size_t> match_positions;
	int32_t uncompressed_size;
	std::string type;
	int32_t id;
	uint32_t murmurhash3;

	SearchResult(SearchResult&& other) noexcept = default;
	SearchResult& operator=(SearchResult&& other) noexcept = default;
	SearchResult() = default;
};

static std::vector<std::optional<uint8_t>> g_search_pattern;
static std::vector<SearchResult> g_search_results;
static std::atomic<bool> g_search_in_progress{ false };
static std::atomic<int> g_files_processed{ 0 };
static std::atomic<int> g_total_files{ 0 };
static std::atomic<int> g_matches_found{ 0 };
static std::atomic<int> g_matches_cleared{ 0 };
static std::mutex g_results_mutex;
static std::future<void> g_search_future;

// Type filtering globals
static std::unordered_set<std::string> g_enabled_types;
static std::unordered_set<std::string> g_discovered_types;
static bool g_types_initialized = false;
static std::atomic<int> g_files_skipped{ 0 };

std::vector<std::optional<uint8_t>> parse_hex_pattern(std::string_view hex_sv) {
	std::vector<std::optional<uint8_t>> result_pattern;
	size_t current_pos = 0;
	size_t view_len = hex_sv.length();

	while (current_pos < view_len) {
		while (current_pos < view_len && std::isspace(static_cast<unsigned char>(hex_sv[current_pos]))) {
			current_pos++;
		}
		if (current_pos == view_len) break;

		size_t token_start = current_pos;
		while (current_pos < view_len && !std::isspace(static_cast<unsigned char>(hex_sv[current_pos]))) {
			current_pos++;
		}

		std::string current_token(hex_sv.substr(token_start, current_pos - token_start));

		if (current_token == "??") {
			result_pattern.push_back(std::nullopt);
		}
		else if (current_token.length() == 2) {
			char* end_ptr;
			long val = std::strtol(current_token.c_str(), &end_ptr, 16);
			if (end_ptr == current_token.c_str() + 2 && val >= 0 && val <= 255) {
				result_pattern.push_back(static_cast<uint8_t>(val));
			}
			else {
				result_pattern.clear();
				return result_pattern;
			}
		}
		else {
			result_pattern.clear();
			return result_pattern;
		}
	}
	return result_pattern;
}

void initialize_file_types(std::map<int, std::unique_ptr<DATManager>>& dat_managers) {
	if (g_types_initialized) return;

	g_discovered_types.clear();
	g_enabled_types.clear();

	// Discover all file types in the DAT files
	for (const auto& pair_entry : dat_managers) {
		if (!pair_entry.second) continue;

		const auto& mft = pair_entry.second->get_MFT();
		for (size_t j = 0; j < mft.size(); ++j) {
			const auto& entry = mft[j];
			std::string type_str = typeToString(entry.type);
			if (!type_str.empty()) {
				g_discovered_types.insert(type_str);
			}
		}
	}

	// Enable all types by default
	g_enabled_types = g_discovered_types;
	g_types_initialized = true;
}

void search_dat_files_worker(DATManager* dat_manager, int dat_alias,
	const BytePatternMatcher& matcher) {
	const auto& mft = dat_manager->get_MFT();
	const size_t current_pattern_size = matcher.get_pattern_size();

	for (size_t j = 0; j < mft.size(); ++j) {
		if (!g_search_in_progress.load(std::memory_order_relaxed)) {
			break;
		}

		const auto& entry = mft[j];

		if (entry.uncompressedSize <= 0 || (current_pattern_size > 0 && static_cast<size_t>(entry.uncompressedSize) < current_pattern_size)) {
			g_files_processed.fetch_add(1, std::memory_order_relaxed);
			continue;
		}

		// Check if this file type should be searched
		std::string type_str = typeToString(entry.type);
		if (g_enabled_types.find(type_str) == g_enabled_types.end()) {
			g_files_processed.fetch_add(1, std::memory_order_relaxed);
			g_files_skipped.fetch_add(1, std::memory_order_relaxed);
			continue;
		}

		uint8_t* file_data = nullptr;
		try {
			file_data = dat_manager->read_file(static_cast<int>(j));
			if (!file_data) {
				g_files_processed.fetch_add(1, std::memory_order_relaxed);
				continue;
			}

			auto matches = matcher.search(file_data, entry.uncompressedSize);

			if (!matches.empty()) {
				SearchResult current_result;
				current_result.file_id = entry.Hash;
				current_result.dat_alias = dat_alias;
				current_result.match_positions = std::move(matches);
				current_result.uncompressed_size = entry.uncompressedSize;
				current_result.type = type_str;
				current_result.id = static_cast<int32_t>(j);
				current_result.murmurhash3 = entry.murmurhash3;

				{
					std::lock_guard<std::mutex> lock(g_results_mutex);
					g_search_results.emplace_back(std::move(current_result));
					g_matches_found.fetch_add(g_search_results.back().match_positions.size(), std::memory_order_relaxed);
				}
			}
		}
		catch (const std::exception& e) {
		}
		catch (...) {
		}

		if (file_data) {
			delete[] file_data;
		}
		g_files_processed.fetch_add(1, std::memory_order_relaxed);
	}
}

void perform_pattern_search(std::map<int, std::unique_ptr<DATManager>>& dat_managers) {
	if (g_search_pattern.empty()) {
		g_search_in_progress.store(false);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(g_results_mutex);
		g_search_results.clear();
		g_search_results.reserve(10000);
	}

	g_files_processed.store(0, std::memory_order_relaxed);
	g_matches_found.store(0, std::memory_order_relaxed);
	g_files_skipped.store(0, std::memory_order_relaxed);

	int total_files_to_scan = 0;
	for (const auto& pair_entry : dat_managers) {
		if (pair_entry.second) {
			total_files_to_scan += static_cast<int>(pair_entry.second->get_MFT().size());
		}
	}
	g_total_files.store(total_files_to_scan);

	if (total_files_to_scan == 0) {
		g_search_in_progress.store(false);
		return;
	}

	BytePatternMatcher matcher(g_search_pattern);

	const size_t max_threads = std::min(static_cast<size_t>(std::thread::hardware_concurrency()), static_cast<size_t>(4));
	std::vector<std::future<void>> active_futures;
	active_futures.reserve(max_threads);

	for (const auto& pair_entry : dat_managers) {
		DATManager* manager_ptr = pair_entry.second.get();
		int alias_val = pair_entry.first;

		if (!g_search_in_progress.load(std::memory_order_relaxed)) break;
		if (!manager_ptr) continue;

		while (active_futures.size() >= max_threads) {
			bool slot_freed = false;
			for (auto it = active_futures.begin(); it != active_futures.end(); ) {
				if (it->wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
					try { it->get(); }
					catch (const std::exception&) {}
					catch (...) {}
					it = active_futures.erase(it);
					slot_freed = true;
					break;
				}
				else {
					++it;
				}
			}
			if (!g_search_in_progress.load(std::memory_order_relaxed)) break;

			if (!slot_freed && active_futures.size() >= max_threads) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
		if (!g_search_in_progress.load(std::memory_order_relaxed)) break;

		active_futures.emplace_back(std::async(std::launch::async, search_dat_files_worker, manager_ptr, alias_val, std::ref(matcher)));
	}

	for (auto& future : active_futures) {
		if (future.valid()) {
			try { future.get(); }
			catch (const std::exception&) {}
			catch (...) {}
		}
	}

	{
		std::lock_guard<std::mutex> lock(g_results_mutex);
		if (g_search_results.size() > 1) {
			try {
				std::sort(std::execution::par_unseq, g_search_results.begin(), g_search_results.end(),
					[](const SearchResult& a, const SearchResult& b) {
						if (a.dat_alias != b.dat_alias) return a.dat_alias < b.dat_alias;
						if (a.id != b.id) return a.id < b.id;
						return a.file_id < b.file_id;
					});
			}
			catch (const std::exception& e) {
				std::sort(g_search_results.begin(), g_search_results.end(),
					[](const SearchResult& a, const SearchResult& b) {
						if (a.dat_alias != b.dat_alias) return a.dat_alias < b.dat_alias;
						if (a.id != b.id) return a.id < b.id;
						return a.file_id < b.file_id;
					});
			}
		}
	}
	g_search_in_progress.store(false);
}

void draw_byte_pattern_search_panel(std::map<int, std::unique_ptr<DATManager>>& dat_managers,
	int& dat_manager_to_show,
	std::unordered_set<uint32_t>& dat_compare_filter_result_out,
	bool& filter_result_changed_out) {

	if (!GuiGlobalConstants::is_byte_search_panel_open) return;

	// Initialize file types if needed
	initialize_file_types(dat_managers);

	if (ImGui::Begin("Byte Pattern Search", &GuiGlobalConstants::is_byte_search_panel_open)) {
		static std::string pattern_input_str;
		static std::string last_valid_pattern_str;

		ImGui::Text("Byte Pattern (e.g., 4A 4B ?? 4D):");
		ImGui::SameLine();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Enter hex bytes (e.g., '66 6e') or '??' for wildcard, separated by spaces.");
		}

		if (ImGui::InputText("##pattern", &pattern_input_str)) {
			auto parsed_temp = parse_hex_pattern(pattern_input_str);
			if (!parsed_temp.empty() || pattern_input_str.empty()) {
				last_valid_pattern_str = pattern_input_str;
			}
		}

		auto current_parsed_pattern = parse_hex_pattern(pattern_input_str);
		if (current_parsed_pattern.empty() && !pattern_input_str.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid pattern format.");
			if (!last_valid_pattern_str.empty() && last_valid_pattern_str != pattern_input_str) {
				ImGui::Text("Last valid input: %s", last_valid_pattern_str.c_str());
			}
		}
		else if (!current_parsed_pattern.empty()) {
			ImGui::Text("Parsed pattern length: %zu bytes", current_parsed_pattern.size());
		}

		ImGui::Separator();

		// File Type Filter Section
		if (!g_discovered_types.empty()) {
			if (ImGui::CollapsingHeader("File Type Filters", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text("Select file types to search:");

				// Helper buttons
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 0.6f));
				if (ImGui::Button("Select All")) {
					g_enabled_types = g_discovered_types;
				}
				ImGui::PopStyleColor();

				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.6f));
				if (ImGui::Button("Deselect All")) {
					g_enabled_types.clear();
				}
				ImGui::PopStyleColor();

				// Display checkboxes in columns for better space usage
				const int columns = 4;
				std::vector<std::string> sorted_types(g_discovered_types.begin(), g_discovered_types.end());
				std::sort(sorted_types.begin(), sorted_types.end());

				if (ImGui::BeginTable("TypeFilters", columns, ImGuiTableFlags_SizingStretchProp)) {
					int column = 0;
					for (const auto& type : sorted_types) {
						if (column == 0) {
							ImGui::TableNextRow();
						}
						ImGui::TableSetColumnIndex(column);

						bool is_enabled = g_enabled_types.find(type) != g_enabled_types.end();
						if (ImGui::Checkbox(type.c_str(), &is_enabled)) {
							if (is_enabled) {
								g_enabled_types.insert(type);
							}
							else {
								g_enabled_types.erase(type);
							}
						}

						column = (column + 1) % columns;
					}
					ImGui::EndTable();
				}

				ImGui::Text("Selected types: %zu/%zu", g_enabled_types.size(), g_discovered_types.size());
			}
			ImGui::Separator();
		}

		if (g_search_future.valid() &&
			g_search_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			try { g_search_future.get(); }
			catch (const std::exception& e) {}
			catch (...) {}
		}

		bool can_start_search = !current_parsed_pattern.empty() && !dat_managers.empty() &&
			!g_search_in_progress.load() && !g_enabled_types.empty();

		if (g_search_in_progress.load()) {
			if (ImGui::Button("Cancel Search")) {
				g_search_in_progress.store(false);
			}
		}
		else {
			ImGui::BeginDisabled(!can_start_search);
			if (ImGui::Button("Start Search")) {
				g_search_pattern = current_parsed_pattern;
				g_search_in_progress.store(true);
				g_files_processed.store(0);
				g_matches_found.store(0);
				g_matches_cleared.store(0);
				g_files_skipped.store(0);

				g_search_future = std::async(std::launch::async, perform_pattern_search, std::ref(dat_managers));
			}
			ImGui::EndDisabled();

			if (!can_start_search && g_enabled_types.empty()) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "(No file types selected)");
			}
		}

		if (g_search_in_progress.load() || g_files_processed.load(std::memory_order_relaxed) > 0 || !g_search_results.empty() || g_matches_cleared.load(std::memory_order_relaxed) > 0) {
			int processed_count = g_files_processed.load(std::memory_order_relaxed);
			int total_count = g_total_files.load(std::memory_order_relaxed);
			int matches_count = g_matches_found.load(std::memory_order_relaxed);
			int cleared_count = g_matches_cleared.load(std::memory_order_relaxed);
			int skipped_count = g_files_skipped.load(std::memory_order_relaxed);

			char progress_buffer[128];

			if (total_count > 0 || g_search_in_progress.load()) {
				float progress_val = 0.0f;
				if (total_count > 0) {
					progress_val = static_cast<float>(processed_count) / total_count;
					progress_val = std::clamp(progress_val, 0.0f, 1.0f);
				}
				else if (g_search_in_progress.load() && processed_count > 0) {
					progress_val = 0.05f;
				}

				if (cleared_count > 0) {
					if (skipped_count > 0) {
						snprintf(progress_buffer, sizeof(progress_buffer), "%d/%d files (%d skipped, some results cleared)", processed_count, total_count > 0 ? total_count : processed_count, skipped_count);
					}
					else {
						snprintf(progress_buffer, sizeof(progress_buffer), "%d/%d files (some results cleared)", processed_count, total_count > 0 ? total_count : processed_count);
					}
				}
				else {
					if (skipped_count > 0) {
						snprintf(progress_buffer, sizeof(progress_buffer), "%d/%d files (%d skipped)", processed_count, total_count > 0 ? total_count : processed_count, skipped_count);
					}
					else {
						snprintf(progress_buffer, sizeof(progress_buffer), "%d/%d files", processed_count, total_count > 0 ? total_count : processed_count);
					}
				}

				if (g_search_in_progress.load() && total_count == 0 && processed_count == 0) {
					ImGui::Text("Initializing search...");
				}
				else {
					ImGui::ProgressBar(progress_val, ImVec2(-1.0f, 0.0f), progress_buffer);
				}
			}

			if (cleared_count > 0) {
				ImGui::Text("Active Matches: %d (Total Found: %d, Manually Cleared: %d)",
					matches_count - cleared_count, matches_count, cleared_count);
			}
			else {
				ImGui::Text("Matches found: %d", matches_count);
			}
		}

		ImGui::Separator();

		size_t current_results_count;
		{
			std::lock_guard<std::mutex> lock(g_results_mutex);
			current_results_count = g_search_results.size();
		}

		if (current_results_count > 0) {
			ImGui::Text("Search Results (%zu files with matches):", current_results_count);

			float bottom_buttons_height = ImGui::GetFrameHeightWithSpacing() * 1.2f;
			float available_height_for_table = ImGui::GetContentRegionAvail().y - bottom_buttons_height;
			if (available_height_for_table < ImGui::GetTextLineHeightWithSpacing() * 5) {
				available_height_for_table = ImGui::GetTextLineHeightWithSpacing() * 5;
			}

			if (ImGui::BeginChild("SearchResultsTableRegion", ImVec2(0, available_height_for_table), false, ImGuiWindowFlags_HorizontalScrollbar)) {
				if (ImGui::BeginTable("SearchResultsDisplayTable", 8,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
					ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {

					ImGui::TableSetupColumn("DAT", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("File Id", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Murmur", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("#Matches", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort);
					ImGui::TableHeadersRow();

					ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
					if (sort_specs != nullptr && sort_specs->SpecsDirty) {
						std::lock_guard<std::mutex> lock(g_results_mutex);

						// Sort the results based on the current sort specification
						if (sort_specs->SpecsCount > 0) {
							const ImGuiTableColumnSortSpecs& sort_spec = sort_specs->Specs[0];

							std::sort(g_search_results.begin(), g_search_results.end(),
								[&sort_spec](const SearchResult& a, const SearchResult& b) {
									bool ascending = sort_spec.SortDirection == ImGuiSortDirection_Ascending;

									switch (sort_spec.ColumnIndex) {
									case 0: // DAT column
										return ascending ? (a.dat_alias < b.dat_alias) : (a.dat_alias > b.dat_alias);
									case 1: // Index column  
										return ascending ? (a.id < b.id) : (a.id > b.id);
									case 2: // File Id column
										return ascending ? (a.file_id < b.file_id) : (a.file_id > b.file_id);
									case 3: // Type column
										return ascending ? (a.type < b.type) : (a.type > b.type);
									case 4: // Size column
										return ascending ? (a.uncompressed_size < b.uncompressed_size) : (a.uncompressed_size > b.uncompressed_size);
									case 5: // Murmur column
										return ascending ? (a.murmurhash3 < b.murmurhash3) : (a.murmurhash3 > b.murmurhash3);
									case 6: // #Matches column
										return ascending ? (a.match_positions.size() < b.match_positions.size()) : (a.match_positions.size() > b.match_positions.size());
									default:
										return false;
									}
								});
						}

						sort_specs->SpecsDirty = false;
					}

					std::lock_guard<std::mutex> lock(g_results_mutex);
					for (size_t i = 0; i < g_search_results.size(); ++i) {
						const auto& result_item = g_search_results[i];
						ImGui::PushID(static_cast<int>(i));

						ImGui::TableNextRow();
						ImGui::TableNextColumn(); ImGui::Text("DAT%d", result_item.dat_alias);
						ImGui::TableNextColumn(); ImGui::Text("%d", result_item.id);
						ImGui::TableNextColumn(); ImGui::Text("0x%08X", result_item.file_id);
						ImGui::TableNextColumn(); ImGui::TextUnformatted(result_item.type.c_str());
						ImGui::TableNextColumn();
						if (result_item.uncompressed_size < 1024) {
							ImGui::Text("%d B", result_item.uncompressed_size);
						}
						else if (result_item.uncompressed_size < 1024 * 1024) {
							ImGui::Text("%.1f KB", result_item.uncompressed_size / 1024.0f);
						}
						else {
							ImGui::Text("%.1f MB", result_item.uncompressed_size / (1024.0f * 1024.0f));
						}
						ImGui::TableNextColumn(); ImGui::Text("%u", result_item.murmurhash3);
						ImGui::TableNextColumn(); ImGui::Text("%zu (hover to see offsets)", result_item.match_positions.size());

						if (ImGui::IsItemHovered() && !result_item.match_positions.empty()) {
							ImGui::BeginTooltip();
							ImGui::Text("Match offsets (max 10 shown):");
							for (size_t pos_idx = 0; pos_idx < std::min(result_item.match_positions.size(), size_t(10)); ++pos_idx) {
								ImGui::Text("0x%zX", result_item.match_positions[pos_idx]);
							}
							if (result_item.match_positions.size() > 10) {
								ImGui::Text("... and %zu more.", result_item.match_positions.size() - 10);
							}
							ImGui::EndTooltip();
						}

						ImGui::TableNextColumn();
						if (ImGui::Button("Show In DAT")) {
							dat_manager_to_show = result_item.dat_alias;
							dat_compare_filter_result_out.clear();
							dat_compare_filter_result_out.insert(result_item.murmurhash3);
							filter_result_changed_out = true;
						}
						ImGui::PopID();
					}
					ImGui::EndTable();
				}
			}
			ImGui::EndChild();

			if (ImGui::Button("Clear all")) {
				std::lock_guard<std::mutex> lock(g_results_mutex);
				int matches_in_cleared_results = 0;
				for (const auto& result_item : g_search_results) {
					matches_in_cleared_results += static_cast<int>(result_item.match_positions.size());
				}
				g_matches_cleared.fetch_add(matches_in_cleared_results, std::memory_order_relaxed);
				g_search_results.clear();
				g_search_results.shrink_to_fit();

				dat_compare_filter_result_out.clear();
				filter_result_changed_out = true;
			}

			ImGui::SameLine();
			if (ImGui::Button("Filter all")) {
				std::lock_guard<std::mutex> lock(g_results_mutex);
				dat_compare_filter_result_out.clear();
				if (!g_search_results.empty()) {
					dat_compare_filter_result_out.reserve(g_search_results.size());
				}
				for (const auto& res_item : g_search_results) {
					dat_compare_filter_result_out.insert(res_item.murmurhash3);
				}
				filter_result_changed_out = true;
			}
		}
	}
	ImGui::End();
}