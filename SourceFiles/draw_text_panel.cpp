#include "pch.h"
#include "draw_text_panel.h"
#include <GuiGlobalConstants.h>

void draw_text_panel(std::string text)
{
    if (GuiGlobalConstants::is_text_panel_open) {
        if (ImGui::Begin("Text panel", &GuiGlobalConstants::is_text_panel_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
            ImGui::Text(text.c_str());
        }
        ImGui::End();
    }
}
