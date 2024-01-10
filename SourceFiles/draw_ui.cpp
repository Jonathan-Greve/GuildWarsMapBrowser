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
#include <draw_gui_window_controller.h>
#include <draw_extract_panel.h>

extern FileType selected_file_type;
extern HSTREAM selected_audio_stream_handle;
extern std::string selected_text_file_str;
extern std::vector<uint8_t> selected_raw_data;
bool dat_manager_to_show_changed = false;
bool dat_compare_filter_result_changed = false;
bool custom_file_info_changed = false;
std::unordered_set<uint32_t> dat_compare_filter_result;

void draw_ui(std::map<int, std::unique_ptr<DATManager>>& dat_managers, int& dat_manager_to_show, MapRenderer* map_renderer, PickingInfo picking_info, 
    std::vector<std::vector<std::string>>& csv_data, int& FPS_target, DX::StepTimer& timer, int& pixels_per_tile_x, int& pixels_per_tile_y, bool& pixels_per_tile_changed)
{
    int initial_dat_manager_to_show = dat_manager_to_show;

    if (! gw_dat_path_set)
    {
        draw_gui_for_open_dat_file();
    }
    else
    {
        draw_gui_window_controller();

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
                draw_right_panel(map_renderer, FPS_target, timer);
            }
            
            draw_extract_panel(pixels_per_tile_x, pixels_per_tile_y, pixels_per_tile_changed);

            dat_compare_filter_result_changed = false;
            draw_dat_compare_panel(dat_managers, dat_manager_to_show, dat_compare_filter_result, dat_compare_filter_result_changed);
            
            custom_file_info_changed = false;
            custom_file_info_changed = draw_file_info_editor_panel(csv_data);

            draw_picking_info(picking_info);

            if (selected_file_type >= ATEXDXT1 && selected_file_type <= ATTXDXTL &&
                  selected_file_type != ATEXDXTA && selected_file_type != ATTXDXTA ||
                selected_file_type == DDS || selected_file_type == FFNA_Type3 ||
                selected_file_type == FFNA_Type2)
            {
                draw_texture_panel(map_renderer);
            }
            else if (selected_file_type == AMP || selected_file_type == SOUND)
            {
                draw_audio_controller_panel(selected_audio_stream_handle);
            }
            else if (selected_file_type == TEXT)
            {
                draw_text_panel(selected_text_file_str);
            }

            if (selected_raw_data.size() > 0)
            {
                draw_hex_editor_panel(selected_raw_data.data(), selected_raw_data.size());
            }
        }
    }

    dat_manager_to_show_changed = dat_manager_to_show != initial_dat_manager_to_show;
}
