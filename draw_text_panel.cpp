#include "pch.h"
#include "draw_text_panel.h"

void draw_text_panel(std::string text)
{
    ImGui::Begin("Audio Control");
    ImGui::Text(text.c_str());
    ImGui::End();
}
