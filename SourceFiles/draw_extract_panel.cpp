#include "pch.h"
#include "draw_extract_panel.h"
#include "GuiGlobalConstants.h"
#include "GWUnpacker.h"
#include "FFNA_ModelFile_Other.h"
#include "DirectXTex/DirectXTex.h"
#include <thread>
#include <atomic>

constexpr int max_pixel_per_tile_dir = 16384;

void draw_extract_panel(ExtractPanelInfo& extract_panel_info, DATManager* dat_manager)
{
	if (GuiGlobalConstants::is_extract_panel_open) {
		if (ImGui::Begin("Extract Panel", &GuiGlobalConstants::is_extract_panel_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
			GuiGlobalConstants::ClampWindowToScreen();

			if (ImGui::CollapsingHeader("Extract maps to image file", ImGuiTreeNodeFlags_DefaultOpen)) {
				static int selected_option = 2;
				const char* options[] = { "Save all maps - top down view", "Save current map - top down view", "Save current map - current view" };

				// Display the combo box with the options
				if (ImGui::Combo("View Options", &selected_option, options, IM_ARRAYSIZE(options))) {
					if (selected_option == 0) {
						extract_panel_info.map_render_extract_map_type = ExtractPanel::AllMapsTopDownOrthographic;
					}
					else if (selected_option == 1) {
						extract_panel_info.map_render_extract_map_type = ExtractPanel::CurrentMapTopDownOrthographic;
					}
					else if (selected_option == 2) {
						extract_panel_info.map_render_extract_map_type = ExtractPanel::CurrentMapNoViewChange;
					}
				}

				// Input for pixels per tile in the x-direction
				ImGui::InputInt("Pixels per Tile X", &extract_panel_info.pixels_per_tile_x, 1, 5, ImGuiInputTextFlags_CharsDecimal);
				extract_panel_info.pixels_per_tile_x = (extract_panel_info.pixels_per_tile_x > max_pixel_per_tile_dir) ? max_pixel_per_tile_dir : (extract_panel_info.pixels_per_tile_x < 1) ? 1 : extract_panel_info.pixels_per_tile_x; // Clamping value between 1 and max_pixel_per_tile_dir

				// Input for pixels per tile in the y-direction
				ImGui::InputInt("Pixels per Tile Y", &extract_panel_info.pixels_per_tile_y, 1, 5, ImGuiInputTextFlags_CharsDecimal);
				extract_panel_info.pixels_per_tile_y = (extract_panel_info.pixels_per_tile_y > max_pixel_per_tile_dir) ? max_pixel_per_tile_dir : (extract_panel_info.pixels_per_tile_y < 1) ? 1 : extract_panel_info.pixels_per_tile_y; // Clamping value between 1 and max_pixel_per_tile_dir

				// Buttons for extraction
				if (ImGui::Button("Extract as DDS")) {
					std::wstring saveDir = OpenDirectoryDialog();
					if (!saveDir.empty())
					{
						extract_panel_info.save_directory = saveDir;
						extract_panel_info.map_render_extract_file_type = ExtractPanel::DDS;
						extract_panel_info.pixels_per_tile_changed = true;
					}

				}

				ImGui::SameLine();

				if (ImGui::Button("Extract as PNG")) {
					std::wstring saveDir = OpenDirectoryDialog();
					if (!saveDir.empty())
					{
						extract_panel_info.save_directory = saveDir;
						extract_panel_info.map_render_extract_file_type = ExtractPanel::PNG;
						extract_panel_info.pixels_per_tile_changed = true;
					}
				}
			}

			if (ImGui::CollapsingHeader("Extract decompressed files", ImGuiTreeNodeFlags_DefaultOpen)) {
				const auto& mft = dat_manager->get_MFT();
				static std::map<int, bool> fileTypeSelections;
				static bool initialized = false;
				static int num_files_to_extract = 0;
				static bool saveToSubfolders = false;
				static bool useMP3Extension = false;
				static bool useTxtExtension = false;
				static bool useDdsExtension = false;

				if (!initialized) {
					for (FileType type : GetAllFileTypes()) {
						fileTypeSelections[static_cast<int>(type)] = true;
						num_files_to_extract += dat_manager->get_num_files_for_type(type);
					}
					initialized = true;
				}

				static std::string num_files_to_extract_ui_str = std::format("Number of files to extract: {}", num_files_to_extract);

				int checkboxCounter = 0;
				for (FileType type : GetAllFileTypes()) {
					std::string typeName = typeToString(type);
					if (typeName.empty() || type == NONE) continue;

					if (checkboxCounter > 0) ImGui::SameLine();

					if (ImGui::Checkbox(typeName.c_str(), &fileTypeSelections[static_cast<int>(type)])) {
						num_files_to_extract = 0;
						for (FileType type : GetAllFileTypes()) {
							if (fileTypeSelections[type]) {
								num_files_to_extract += dat_manager->get_num_files_for_type(type);
							}
						}
						num_files_to_extract_ui_str = std::format("Number of files to extract: {}", num_files_to_extract);
					}

					checkboxCounter++;
					if (checkboxCounter % 5 == 0) checkboxCounter = 0;
				}

				ImGui::Checkbox("Save each file type into its own subfolder", &saveToSubfolders);
				ImGui::Checkbox("Use .mp3 extension for AMP and SOUND files", &useMP3Extension);
				ImGui::Checkbox("Use .txt extension for Text files", &useTxtExtension);
				ImGui::Checkbox("Use .dds extension for DDS files", &useDdsExtension);

				ImGui::Text(num_files_to_extract_ui_str.c_str());

				if (ImGui::Button("Extract selected file types")) {
					std::wstring saveDir = OpenDirectoryDialog();
					if (!saveDir.empty()) {
						const auto num_threads = std::thread::hardware_concurrency();
						std::vector<std::jthread> threads(num_threads);

						for (std::size_t t = 0; t < num_threads; ++t) {
							threads[t] = std::jthread([&, t]() {
								for (std::size_t i = t; i < mft.size(); i += num_threads) {
									const auto& entry = mft[i];
									if (fileTypeSelections[entry.type]) {
										std::wstring subfolder = L"";
										if (saveToSubfolders) {
											subfolder = typeToWString(entry.type);
											std::filesystem::create_directories(std::filesystem::path(saveDir) / subfolder);
										}

										std::wstring extension = L".gwraw";
										if (useMP3Extension && (entry.type == AMP || entry.type == SOUND)) {
											extension = L".mp3";
										}
										else if (useTxtExtension && (entry.type == TEXT)) {
											extension = L".txt";
										}
										else if (useDdsExtension && (entry.type == DDS)) {
											extension = L".dds";
										}

										const auto filename = std::format(L"{}_{}_{}_{}{}", i, entry.Hash, entry.murmurhash3, typeToWString(entry.type), extension);
										if (!std::filesystem::exists(std::filesystem::path(saveDir) / subfolder / filename)) {
											dat_manager->save_raw_decompressed_data_to_file(i, std::filesystem::path(saveDir) / subfolder / filename);
										}
									}
								}
								});
						}
					}
				}
				ImGui::Separator();
				ImGui::Text("Extract All Textures:");
				if (ImGui::Button("Extract All Textures as PNG")) {
					std::wstring saveDir = OpenDirectoryDialog();
					if (!saveDir.empty()) {
						extract_panel_info.save_directory = saveDir;
						extract_panel_info.extract_all_textures_requested = true;
					}

				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Extracts all texture files (ATEX, ATTX, DDS) from the currently loaded DAT file to the selected directory as PNG files.");
				}

				ImGui::Separator();
				ImGui::Text("Extract Inventory Icons:");

				static std::atomic<bool> is_extracting_icons{false};
				static std::atomic<int> icons_extracted{0};
				static std::atomic<int> total_icons_to_extract{0};

				if (is_extracting_icons.load()) {
					ImGui::Text("Extracting: %d / %d", icons_extracted.load(), total_icons_to_extract.load());
				}
				else {
					if (ImGui::Button("Extract All Inventory Icons as PNG")) {
						std::wstring saveDir = OpenDirectoryDialog();
						if (!saveDir.empty()) {
							is_extracting_icons.store(true);
							icons_extracted.store(0);
							total_icons_to_extract.store(0);

							// Launch extraction in a background thread
							std::thread([saveDir, dat_manager]() {
								const auto& mft = dat_manager->get_MFT();

								// First pass: count FFNA_Type2 files
								int type2_count = 0;
								for (size_t i = 0; i < mft.size(); ++i) {
									if (mft[i].type == FFNA_Type2) {
										type2_count++;
									}
								}
								total_icons_to_extract.store(type2_count);

								// Second pass: extract icons
								for (size_t i = 0; i < mft.size(); ++i) {
									if (!is_extracting_icons.load()) break; // Allow cancellation

									if (mft[i].type == FFNA_Type2) {
										try {
											// Check if this is an "other" format model with inline textures
											if (dat_manager->is_other_model_format(static_cast<int>(i))) {
												auto model = dat_manager->parse_ffna_model_file_other(static_cast<int>(i));

												if (model.has_inline_textures) {
													auto inline_textures = model.GetAllInlineTextures();

													for (size_t tex_idx = 0; tex_idx < inline_textures.size(); ++tex_idx) {
														const auto& tex = inline_textures[tex_idx];
														if (tex.width > 0 && tex.height > 0 && !tex.rgba_data.empty()) {
															// Create filename using hash for frontend lookup
															std::wstring filename = std::format(L"itemIcon_{}.png", mft[i].Hash);

															// If multiple textures, add index
															if (inline_textures.size() > 1) {
																filename = std::format(L"itemIcon_{}_{}.png", mft[i].Hash, tex_idx);
															}

															std::filesystem::path filepath = std::filesystem::path(saveDir) / filename;

															// Skip if file already exists
															if (std::filesystem::exists(filepath)) {
																continue;
															}

															// Create DirectX Image from BGRA data (GW textures are stored as BGRA)
															DirectX::Image image;
															image.width = tex.width;
															image.height = tex.height;
															image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
															image.rowPitch = tex.width * 4;
															image.slicePitch = image.rowPitch * tex.height;
															image.pixels = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(tex.rgba_data.data()));

															// Save to PNG
															DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_FORCE_SRGB,
																DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), filepath.c_str());
														}
													}
												}
											}
										}
										catch (...) {
											// Skip files that fail to parse
										}

										icons_extracted.fetch_add(1);
									}
								}

								is_extracting_icons.store(false);
							}).detach();
						}
					}
				}

				if (ImGui::IsItemHovered() && !is_extracting_icons.load())
				{
					ImGui::SetTooltip("Extracts inline textures (inventory icons) from all FFNA Type 2 \"Other\" model files.\nNaming format: itemIcon_{hash}.png");
				}

				if (is_extracting_icons.load()) {
					ImGui::SameLine();
					if (ImGui::Button("Stop Icon Extraction")) {
						is_extracting_icons.store(false);
					}
				}
			}
		}
		ImGui::End();
	}
}

