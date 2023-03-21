#include "pch.h"
#include "draw_left_panel.h"

void draw_left_panel()
{
    constexpr auto window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    // set up the left panel
    ImGui::SetNextWindowPos(ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::left_panel_width,
                                    ImGui::GetIO().DisplaySize.y - 2 * GuiGlobalConstants::panel_padding));
    ImGui::PushStyleVar(
      ImGuiStyleVar_WindowPadding,
      ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding)); // add padding
    ImGui::Begin("Left Panel", NULL, window_flags);

    // draw the contents of the left panel
    ImGui::Text("This is the left panel");

    ImGui::End();
    ImGui::PopStyleVar();
}
