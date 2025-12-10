#include "pch.h"
#include "draw_text_panel.h"
#include <GuiGlobalConstants.h>

void draw_text_panel(std::string text)
{
    if (!GuiGlobalConstants::is_text_panel_open) return;

    if (ImGui::Begin("Text Panel", &GuiGlobalConstants::is_text_panel_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
        GuiGlobalConstants::ClampWindowToScreen();
        if (text.empty()) {
            ImGui::TextWrapped("No text loaded.");
            ImGui::TextWrapped("Select a text file (TEXT) from the DAT browser to view text content here.");
        }
        else {
            ImGui::TextWrapped("%s", text.c_str());
        }
    }
    ImGui::End();
}
