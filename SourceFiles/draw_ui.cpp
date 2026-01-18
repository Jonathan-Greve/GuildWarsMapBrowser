#include "pch.h"
#include "draw_ui.h"
#include "draw_dat_browser.h"
#include "draw_gui_for_open_dat_file.h"
#include "draw_dat_load_progress_bar.h"
#include "draw_left_panel.h"
#include "draw_right_panel.h"
#include "draw_texture_panel.h"
#include "draw_audio_controller_panel.h"
#include "draw_text_panel.h"
#include "draw_hex_editor_panel.h"
#include "draw_picking_info.h"
#include "draw_dat_compare_panel.h"
#include "draw_file_info_editor_panel.h"
#include "draw_pathfinding_panel.h"
#include "draw_animation_panel.h"
#include <draw_gui_window_controller.h>
#include <draw_extract_panel.h>
#include <byte_pattern_search_panel.h>
#include <windows.h>

extern FileType selected_file_type;
extern HSTREAM selected_audio_stream_handle;
extern std::string selected_text_file_str;
extern std::vector<uint8_t> selected_raw_data;
bool dat_manager_to_show_changed = false;
bool dat_compare_filter_result_changed = false;
bool custom_file_info_changed = false;
std::unordered_set<uint32_t> dat_compare_filter_result;

void draw_ui(std::map<int, std::unique_ptr<DATManager>>& dat_managers, int& dat_manager_to_show, MapRenderer* map_renderer, PickingInfo picking_info,
	std::vector<std::vector<std::string>>& csv_data, int& FPS_target, DX::StepTimer& timer, ExtractPanelInfo& extract_panel_info, bool& msaa_changed,
	int& msaa_level_index, const std::vector<std::pair<int, int>>& msaa_levels, std::unordered_map<int, std::vector<int>>& hash_index)
{
	int initial_dat_manager_to_show = dat_manager_to_show;

	if (!gw_dat_path_set)
	{
		draw_gui_for_open_dat_file();
	}
	else
	{
		// Main menu bar
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("View")) {
				bool changed = false;
				changed |= ImGui::MenuItem("DAT Browser", NULL, &GuiGlobalConstants::is_dat_browser_open);
				changed |= ImGui::MenuItem("Left Panel (File Info)", NULL, &GuiGlobalConstants::is_left_panel_open);
				changed |= ImGui::MenuItem("Right Panel (Render)", NULL, &GuiGlobalConstants::is_right_panel_open);
				changed |= ImGui::MenuItem("Window Controller", NULL, &GuiGlobalConstants::is_window_controller_open);
				ImGui::Separator();
				changed |= ImGui::MenuItem("Hex Editor", NULL, &GuiGlobalConstants::is_hex_editor_open);
				changed |= ImGui::MenuItem("Texture Panel", NULL, &GuiGlobalConstants::is_texture_panel_open);
				changed |= ImGui::MenuItem("Picking Info", NULL, &GuiGlobalConstants::is_picking_panel_open);
				changed |= ImGui::MenuItem("Pathfinding Map", NULL, &GuiGlobalConstants::is_pathfinding_panel_open);
				ImGui::Separator();
				changed |= ImGui::MenuItem("Audio Controller", NULL, &GuiGlobalConstants::is_audio_controller_open);
				changed |= ImGui::MenuItem("Animation Controller", NULL, &GuiGlobalConstants::is_animation_panel_open);
				changed |= ImGui::MenuItem("Text Panel", NULL, &GuiGlobalConstants::is_text_panel_open);
				ImGui::Separator();
				changed |= ImGui::MenuItem("Extract Panel", NULL, &GuiGlobalConstants::is_extract_panel_open);
				changed |= ImGui::MenuItem("Compare Panel", NULL, &GuiGlobalConstants::is_compare_panel_open);
				changed |= ImGui::MenuItem("Byte Search", NULL, &GuiGlobalConstants::is_byte_search_panel_open);
				changed |= ImGui::MenuItem("Custom File Info", NULL, &GuiGlobalConstants::is_custom_file_info_editor_open);
				ImGui::Separator();
				if (ImGui::MenuItem("DAT Browser Movable/Resizeable", NULL, &GuiGlobalConstants::is_dat_browser_movable)) {
					GuiGlobalConstants::is_dat_browser_resizeable = GuiGlobalConstants::is_dat_browser_movable;
					changed = true;
				}
				if (changed) GuiGlobalConstants::SaveSettings();
				ImGui::Separator();
				if (ImGui::MenuItem("Exit")) {
					PostQuitMessage(0);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Layout")) {
				if (ImGui::MenuItem("Reset Window Visibility")) {
					GuiGlobalConstants::ResetToDefaults();
					GuiGlobalConstants::SaveSettings();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Hide All", NULL, GuiGlobalConstants::hide_all)) {
					GuiGlobalConstants::SetHideAll(!GuiGlobalConstants::hide_all);
					GuiGlobalConstants::SaveSettings();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		if (GuiGlobalConstants::is_window_controller_open) {
			draw_gui_window_controller();
		}

		const auto& initialization_state = dat_managers[dat_manager_to_show]->m_initialization_state;
		const auto& dat_files_read = dat_managers[dat_manager_to_show]->get_num_files_type_read();
		const auto& dat_total_files = dat_managers[dat_manager_to_show]->get_num_files();

		if (initialization_state == InitializationState::Started)
		{
			draw_dat_load_progress_bar(dat_files_read, dat_total_files);
		}
		if (initialization_state == InitializationState::Completed)
		{
			draw_data_browser(dat_managers[dat_manager_to_show].get(), map_renderer, dat_manager_to_show_changed, dat_compare_filter_result, dat_compare_filter_result_changed, csv_data, custom_file_info_changed);

			if (GuiGlobalConstants::is_left_panel_open) {
				draw_left_panel(map_renderer);
			}
			if (GuiGlobalConstants::is_right_panel_open) {
				draw_right_panel(map_renderer, FPS_target, timer, msaa_changed, msaa_level_index, msaa_levels);
			}

			draw_extract_panel(extract_panel_info, dat_managers[dat_manager_to_show].get());

			dat_compare_filter_result_changed = false;
			draw_dat_compare_panel(dat_managers, dat_manager_to_show, dat_compare_filter_result, dat_compare_filter_result_changed);

			// shares dat_compare_filter_result_changed and dat_compare_filter_result. So might not work when using both compare and byte pattern search at the same time.
			// But lets be honest, no one uses the dat compare anyways... hahaha. Right?
			draw_byte_pattern_search_panel(dat_managers, dat_manager_to_show, dat_compare_filter_result, dat_compare_filter_result_changed);

			custom_file_info_changed = false;
			custom_file_info_changed = draw_file_info_editor_panel(csv_data);

			draw_picking_info(picking_info, map_renderer, dat_managers[dat_manager_to_show].get(), hash_index);

			// Always draw these panels when enabled - they show helpful messages when no content is loaded
			draw_texture_panel(map_renderer);
			draw_pathfinding_panel(map_renderer);
			draw_audio_controller_panel(selected_audio_stream_handle);
			draw_animation_panel(dat_managers);
			draw_text_panel(selected_text_file_str);
			draw_hex_editor_panel(selected_raw_data.data(), static_cast<int>(selected_raw_data.size()));
		}
	}

	dat_manager_to_show_changed = dat_manager_to_show != initial_dat_manager_to_show;
}

