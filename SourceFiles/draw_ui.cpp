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

extern FileType selected_file_type;
extern HSTREAM selected_audio_stream_handle;
extern std::string selected_text_file_str;
extern std::vector<uint8_t> selected_raw_data;

void draw_ui(std::map<int, std::unique_ptr<DATManager>>& dat_managers, MapRenderer* map_renderer, PickingInfo picking_info)
{

    if (! gw_dat_path_set)
    {
        draw_gui_for_open_dat_file();
    }
    else
    {
        const auto& initialization_state = dat_managers[0]->m_initialization_state;
        const auto& dat_files_read = dat_managers[0]->get_num_files_type_read();
        const auto& dat_total_files = dat_managers[0]->get_num_files();

        if (initialization_state == InitializationState::Started)
        {
            draw_dat_load_progress_bar(dat_files_read, dat_total_files);
        }
        if (initialization_state == InitializationState::Completed)
        {
            draw_data_browser(dat_managers[0].get(), map_renderer);
            draw_left_panel(map_renderer);
            draw_right_panel(map_renderer);
            draw_dat_compare_panel(dat_managers);

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
}
