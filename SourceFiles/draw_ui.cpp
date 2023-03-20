#include "pch.h"
#include "draw_ui.h"
#include "draw_dat_browser.h"
#include "draw_gui_for_open_dat_file.h"
#include "draw_dat_load_progress_bar.h"

void draw_ui(InitializationState initialization_state, int dat_files_to_read, int dat_total_files,
             DATManager& dat_manager, MapRenderer* map_renderer)
{
    //constexpr auto window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    //// set up the left panel
    //ImGui::SetNextWindowPos(ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding));
    //ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::left_panel_width,
    //                                ImGui::GetIO().DisplaySize.y - 2 * GuiGlobalConstants::panel_padding));
    //ImGui::PushStyleVar(
    //  ImGuiStyleVar_WindowPadding,
    //  ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding)); // add padding
    //ImGui::Begin("Left Panel", NULL, window_flags);

    //// draw the contents of the left panel
    //ImGui::Text("This is the left panel");

    //ImGui::End();

    //// set up the right panel
    //ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::left_panel_width -
    //                                 GuiGlobalConstants::panel_padding,
    //                               GuiGlobalConstants::panel_padding));
    //ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width,
    //                                ImGui::GetIO().DisplaySize.y - 2 * GuiGlobalConstants::panel_padding));
    //ImGui::Begin("Right Panel", NULL, window_flags);

    //// draw the contents of the right panel
    //ImGui::Text("This is the right panel");

    //ImGui::End();
    //ImGui::PopStyleVar();

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
        }
    }
}
