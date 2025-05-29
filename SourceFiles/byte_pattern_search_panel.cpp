#include "pch.h"
#include "byte_pattern_search_panel.h"
#include <filesystem>
// #include <draw_dat_load_progress_bar.h> // Assuming this is part of your project
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
#include <cstdio> // For snprintf if not C++20's std::format

// Forward declarations if these are not in pch.h or other common headers for this file
// struct MFTEntry;
// class DATManager;
// #include "imgui.h"
// #include "imgui_stdlib.h" // Often needed for ImGui::InputText with std::string


class BytePatternMatcher {
private:
	std::vector<uint8_t> pattern;
	std::array<int, 256> bad_char_table;

	void build_bad_char_table() {
		bad_char_table.fill(-1);
		if (pattern.empty()) return;
		for (size_t i = 0; i < pattern.size(); ++i) {
			bad_char_table[pattern[i]] = static_cast<int>(i);
		}
	}

public:
	BytePatternMatcher(const std::vector<uint8_t>& pat) : pattern(pat) {
		build_bad_char_table();
	}

	size_t get_pattern_size() const {
		return pattern.size();
	}

	std::vector<size_t> search(std::vector<uint8_t> data) const {
		std::vector<size_t> matches;
		if (pattern.empty() || data.size() < pattern.size()) {
			return matches;
		}

		size_t m = pattern.size();
		size_t n = data.size();
		size_t shift = 0;

		while (shift <= (n - m)) {
			int j = static_cast<int>(m - 1);
			while (j >= 0) {
				if (pattern[static_cast<size_t>(j)] != data[shift + static_cast<size_t>(j)]) {
					break;
				}
				j--;
			}

			if (j < 0) {
				matches.push_back(shift);
				size_t next_char_in_text_idx = shift + m;
				if (next_char_in_text_idx < n) {
					int char_in_pattern_idx = bad_char_table[data[next_char_in_text_idx]];
					shift += (char_in_pattern_idx >= 0) ? (m - static_cast<size_t>(char_in_pattern_idx)) : m;
				}
				else {
					shift += 1;
				}
			}
			else {
				int char_in_pattern_idx = bad_char_table[data[shift + static_cast<size_t>(j)]];
				int shift_val = j - char_in_pattern_idx;
				shift += (shift_val > 0) ? static_cast<size_t>(shift_val) : 1;
			}
		}
		return matches;
	}
};

struct SearchResult {
	uint32_t file_id;
	uint32_t dat_alias;
	std::vector<size_t> match_positions;
	__int32 uncompressed_size;
	std::string type;
	__int32 id;
	uint32_t murmurhash3;
};

static std::vector<uint8_t> g_search_pattern;
static std::vector<SearchResult> g_search_results;
static std::atomic<bool> g_search_in_progress{ false };
static std::atomic<int> g_files_processed{ 0 };
static std::atomic<int> g_total_files{ 0 };
static std::atomic<int> g_matches_found{ 0 };
static std::atomic<int> g_matches_cleared{ 0 };  // NEW: Track cleared matches
static std::mutex g_results_mutex;
static std::future<void> g_search_future;

std::vector<uint8_t> parse_hex_pattern(const std::string& hex_str) {
	std::vector<uint8_t> bytes;
	std::istringstream iss(hex_str);
	std::string hex_byte_token;

	while (iss >> hex_byte_token) {
		if (hex_byte_token.length() == 2) {
			try {
				uint8_t byte_val = static_cast<uint8_t>(std::stoul(hex_byte_token, nullptr, 16));
				bytes.push_back(byte_val);
			}
			catch (...) {
				// Skip invalid hex tokens
			}
		}
	}
	return bytes;
}

void search_dat_files_worker(DATManager* dat_manager, int dat_alias,
	const BytePatternMatcher& matcher,
	std::vector<SearchResult>& local_results) {
	const auto& mft = dat_manager->get_MFT();
	local_results.reserve(mft.size() / 100 + 1);

	for (int i = 0; i < mft.size(); i++) {
		if (!g_search_in_progress.load(std::memory_order_relaxed)) {
			break;
		}

		const auto& entry = mft[i];

		uint8_t* file_data = nullptr;
		try {
			if (entry.uncompressedSize <= 0) {
				g_files_processed.fetch_add(1, std::memory_order_relaxed);
				continue;
			}
			size_t current_file_true_size = static_cast<size_t>(entry.uncompressedSize);

			file_data = dat_manager->read_file(i);
			auto selected_raw_data = std::vector<uint8_t>(file_data, file_data + entry.uncompressedSize);
			delete file_data;

			if (!selected_raw_data.empty()) {
				auto all_matches = matcher.search(selected_raw_data);

				if (!all_matches.empty()) {
					SearchResult result;
					result.file_id = entry.Hash;
					result.dat_alias = dat_alias;
					result.match_positions = std::move(all_matches);
					result.uncompressed_size = entry.uncompressedSize;
					result.type = typeToString(entry.type);  // Make sure this function exists and works
					result.id = i;
					result.murmurhash3 = entry.murmurhash3;

					// Add individual results as they're found for real-time updates
					{
						std::lock_guard<std::mutex> lock(g_results_mutex);
						g_search_results.push_back(result);
						g_matches_found.fetch_add(result.match_positions.size(), std::memory_order_relaxed);
					}
				}
			}
		}
		catch (const std::bad_alloc&) {
			// Log or handle bad_alloc if necessary
		}
		catch (...) {
			// Log or handle other errors if necessary
		}

		g_files_processed.fetch_add(1, std::memory_order_relaxed);

		if (g_files_processed.load(std::memory_order_relaxed) % 100 == 0) {
			std::this_thread::yield();
		}
	}
}


void perform_pattern_search(std::map<int, std::unique_ptr<DATManager>>& dat_managers) {
	if (g_search_pattern.empty()) {
		return;
	}

	// Clear results at start
	{
		std::lock_guard<std::mutex> lock(g_results_mutex);
		g_search_results.clear();
		g_search_results.shrink_to_fit();
	}

	g_files_processed.store(0);
	g_matches_found.store(0);
	g_matches_cleared.store(0);  // Reset cleared counter
	g_total_files.store(0);

	int total = 0;
	for (const auto& pair_alias_manager : dat_managers) {
		if (pair_alias_manager.second) {
			total += pair_alias_manager.second->get_MFT().size();
		}
	}
	g_total_files.store(total);
	if (total == 0) {
		return;
	}

	BytePatternMatcher matcher(g_search_pattern);

	const size_t num_hardware_threads = std::thread::hardware_concurrency();
	const size_t max_threads = num_hardware_threads > 0 ? num_hardware_threads : size_t(1);
	std::vector<std::future<void>> futures;  // Changed to void since we're adding results directly
	futures.reserve(dat_managers.size());

	auto it = dat_managers.begin();
	while (it != dat_managers.end() || !futures.empty()) {
		while (futures.size() < max_threads && it != dat_managers.end()) {
			if (!g_search_in_progress.load()) break;

			const auto& [alias, manager_ptr] = *it;
			if (manager_ptr) {
				futures.push_back(std::async(std::launch::async,
					[manager = manager_ptr.get(), alias, &matcher]() {
						std::vector<SearchResult> local_results;  // Not used anymore, but kept for compatibility
						if (g_search_in_progress.load()) {
							search_dat_files_worker(manager, alias, matcher, local_results);
						}
					}));
			}
			++it;
		}

		if (!g_search_in_progress.load() && it == dat_managers.end() && futures.empty()) {
			break;
		}

		for (size_t i = 0; i < futures.size(); ) {
			if (!g_search_in_progress.load() || futures[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
				try {
					futures[i].get();
				}
				catch (const std::exception&) {
					// Log error if necessary
				}
				futures.erase(futures.begin() + i);
			}
			else {
				i++;
			}
		}

		if (it == dat_managers.end() && futures.empty()) break;

		if (futures.size() >= max_threads && it != dat_managers.end()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		else if (futures.empty() && it == dat_managers.end()) {
			break;
		}
	}

	// Sort results at the end
	{
		std::lock_guard<std::mutex> lock(g_results_mutex);
		if (!g_search_results.empty()) {
			std::sort(std::execution::par_unseq, g_search_results.begin(), g_search_results.end(),
				[](const SearchResult& a, const SearchResult& b) {
					if (a.dat_alias != b.dat_alias) return a.dat_alias < b.dat_alias;
					return a.file_id < b.file_id;
				});
		}
	}
}

void draw_byte_pattern_search_panel(std::map<int, std::unique_ptr<DATManager>>& dat_managers,
	int& dat_manager_to_show,
	std::unordered_set<uint32_t>& dat_compare_filter_result_out,
	bool& filter_result_changed_out) {

	if (GuiGlobalConstants::is_byte_search_panel_open) {
		if (ImGui::Begin("Byte Pattern Search", &GuiGlobalConstants::is_byte_search_panel_open)) {

			static std::string pattern_input = "";
			static std::string last_valid_pattern = "";

			ImGui::Text("Byte Pattern (hex, space-separated):");
			ImGui::SameLine();
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Enter hex bytes separated by spaces (e.g., '66 6e 61 02 b8')");
			}

			if (ImGui::InputText("##pattern", &pattern_input, ImGuiInputTextFlags_CharsUppercase)) {
				auto parsed = parse_hex_pattern(pattern_input);
				if (!parsed.empty() || pattern_input.empty()) {
					last_valid_pattern = pattern_input;
				}
			}

			auto current_pattern = parse_hex_pattern(pattern_input);
			if (current_pattern.empty() && !pattern_input.empty()) {
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid pattern format");
				if (!last_valid_pattern.empty() && last_valid_pattern != pattern_input) {
					ImGui::Text("Last valid: %s", last_valid_pattern.c_str());
				}
			}
			else if (!current_pattern.empty()) {
				ImGui::Text("Pattern length: %zu bytes", current_pattern.size());
			}

			ImGui::Separator();

			if (g_search_future.valid() &&
				g_search_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
				try {
					g_search_future.get();
				}
				catch (const std::exception&) {
					// Log or display error
				}
				g_search_in_progress.store(false);
			}

			bool can_search = !current_pattern.empty() && !dat_managers.empty() && !g_search_in_progress.load();

			if (g_search_in_progress.load()) {
				if (ImGui::Button("Cancel Search")) {
					g_search_in_progress.store(false);
				}
			}
			else {
				if (ImGui::Button("Start Search") && can_search) {
					g_search_pattern = current_pattern;
					g_search_in_progress.store(true);
					g_files_processed.store(0);
					g_matches_found.store(0);
					g_matches_cleared.store(0);  // Reset cleared counter when starting new search
					g_total_files.store(0);
					// Clear results will be done in perform_pattern_search

					g_search_future = std::async(std::launch::async, [&dat_managers]() {
						perform_pattern_search(dat_managers);
						});
				}
			}

			if (g_search_in_progress.load() || g_files_processed.load(std::memory_order_relaxed) > 0) {
				int processed = g_files_processed.load(std::memory_order_relaxed);
				int total = g_total_files.load(std::memory_order_relaxed);
				int matches = g_matches_found.load(std::memory_order_relaxed);
				int cleared = g_matches_cleared.load(std::memory_order_relaxed);

				if (total > 0) {
					float progress = static_cast<float>(processed) / total;
					std::string progress_text;
					if (cleared > 0) {
						progress_text = std::format("{}/{} files ({} matches cleared)", processed, total, cleared);
					}
					else {
						progress_text = std::format("{}/{} files", processed, total);
					}
					ImGui::ProgressBar(progress, ImVec2(-1, 0), progress_text.c_str());
				}
				else if (g_search_in_progress.load()) {
					ImGui::Text("Initializing search...");
				}

				// Show current matches (excluding cleared ones)
				if (cleared > 0) {
					ImGui::Text("Active matches: %d (Total found: %d, Cleared: %d)",
						matches - cleared, matches, cleared);
				}
				else {
					ImGui::Text("Matches found: %d", matches);
				}
			}

			ImGui::Separator();

			// Get current results count (thread-safe)
			size_t results_count = 0;
			{
				std::lock_guard<std::mutex> lock(g_results_mutex);
				results_count = g_search_results.size();
			}

			if (results_count > 0) {
				ImGui::Text("Search Results (%zu files with matches):", results_count);

				// Create a child window for the table with scrolling
				// Calculate height for the table - leave space for buttons at bottom
				float button_height = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
				float available_height = ImGui::GetContentRegionAvail().y - button_height * 2; // Space for 2 rows of buttons

				if (ImGui::BeginChild("SearchResultsTable", ImVec2(0, available_height), false, ImGuiWindowFlags_HorizontalScrollbar)) {
					if (ImGui::BeginTable("SearchResults", 8,
						ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
						ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {

						ImGui::TableSetupColumn("DAT", ImGuiTableColumnFlags_WidthFixed, 50);
						ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80);
						ImGui::TableSetupColumn("File Id", ImGuiTableColumnFlags_WidthFixed, 100);
						ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
						ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
						ImGui::TableSetupColumn("MurMur", ImGuiTableColumnFlags_WidthFixed, 100);
						ImGui::TableSetupColumn("Matches", ImGuiTableColumnFlags_WidthFixed, 70);
						ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80);
						ImGui::TableHeadersRow();

						// Thread-safe iteration over results
						std::lock_guard<std::mutex> lock(g_results_mutex);
						for (size_t i = 0; i < g_search_results.size(); ++i) {
							const auto& result = g_search_results[i];
							ImGui::PushID(static_cast<int>(i));

							ImGui::TableNextRow();
							ImGui::TableNextColumn(); ImGui::Text("DAT%d", result.dat_alias);
							ImGui::TableNextColumn(); ImGui::Text("%d", result.id);
							ImGui::TableNextColumn(); ImGui::Text("0x%08X", result.file_id);
							ImGui::TableNextColumn(); ImGui::Text("%s", result.type.c_str());  // Fixed: use .c_str()
							ImGui::TableNextColumn();
							if (result.uncompressed_size < 1024) ImGui::Text("%d B", result.uncompressed_size);
							else if (result.uncompressed_size < 1024 * 1024) ImGui::Text("%.1f KB", result.uncompressed_size / 1024.0f);
							else ImGui::Text("%.1f MB", result.uncompressed_size / (1024.0f * 1024.0f));
							ImGui::TableNextColumn(); ImGui::Text("%u", result.murmurhash3);  // Fixed: use %u for uint32_t
							ImGui::TableNextColumn(); ImGui::Text("%zu", result.match_positions.size());
							if (ImGui::IsItemHovered() && !result.match_positions.empty()) {
								ImGui::BeginTooltip();
								ImGui::Text("Match positions (offset):");
								for (size_t pos_idx = 0; pos_idx < std::min(result.match_positions.size(), size_t(10)); ++pos_idx) {
									ImGui::Text("0x%zX", result.match_positions[pos_idx]);
								}
								if (result.match_positions.size() > 10) ImGui::Text("... and %zu more", result.match_positions.size() - 10);
								ImGui::EndTooltip();
							}

							ImGui::TableNextColumn();
							if (ImGui::Button("Show")) {
								dat_manager_to_show = result.dat_alias;
								dat_compare_filter_result_out.clear();
								dat_compare_filter_result_out.insert(result.murmurhash3);
								filter_result_changed_out = true;
							}

							ImGui::PopID();
						}
						ImGui::EndTable();
					}
				}
				ImGui::EndChild();

				// Buttons are now always visible at the bottom
				if (ImGui::Button("Clear Results")) {
					std::lock_guard<std::mutex> lock(g_results_mutex);
					// Count the matches we're about to clear
					int matches_to_clear = 0;
					for (const auto& result : g_search_results) {
						matches_to_clear += result.match_positions.size();
					}
					// Update the cleared counter
					g_matches_cleared.fetch_add(matches_to_clear, std::memory_order_relaxed);

					g_search_results.clear();
					g_search_results.shrink_to_fit();
					// DON'T reset g_files_processed or g_matches_found here - they show total progress
				}

				ImGui::SameLine();
				if (ImGui::Button("Filter All Matches")) {
					std::lock_guard<std::mutex> lock(g_results_mutex);
					dat_compare_filter_result_out.clear();
					for (const auto& res : g_search_results) {
						dat_compare_filter_result_out.insert(res.murmurhash3);
					}
					filter_result_changed_out = true;
				}
			}
		}
		ImGui::End();
	}
}