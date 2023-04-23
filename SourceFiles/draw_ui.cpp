#include "pch.h"
#include "draw_ui.h"
#include "draw_dat_browser.h"
#include "draw_gui_for_open_dat_file.h"
#include "draw_dat_load_progress_bar.h"
#include "draw_left_panel.h"
#include "draw_right_panel.h"
#include "draw_texture_panel.h"

extern FileType selected_file_type;

void draw_ui(InitializationState initialization_state, int dat_files_to_read, int dat_total_files,
             DATManager& dat_manager, MapRenderer* map_renderer)
{

    if (! gw_dat_path_set)
    {
        draw_gui_for_open_dat_file();
    }
    else
    {
        if (initialization_state == InitializationState::Started)
        {
            draw_dat_load_progress_bar(dat_files_to_read, dat_total_files);
        }
        if (initialization_state == InitializationState::Completed)
        {
            draw_data_browser(dat_manager, map_renderer);
            draw_left_panel(map_renderer);
            draw_right_panel(map_renderer);

            if (selected_file_type >= ATEXDXT1 && selected_file_type <= ATTXDXTL &&
                  selected_file_type != ATEXDXTA && selected_file_type != ATTXDXTA ||
                selected_file_type == DDS || selected_file_type == FFNA_Type3 ||
                selected_file_type == FFNA_Type2)
            {
                draw_texture_panel(map_renderer);
            }
        }
    }
}
