#include "pch.h"
#include "draw_hex_editor_panel.h"
#include "imgui_memory_editor.h"
#include "GuiGlobalConstants.h"

void draw_hex_editor_panel(unsigned char* data, int data_size)
{
    if (!GuiGlobalConstants::is_hex_editor_open) return;

    static MemoryEditor mem_edit;
    mem_edit.ReadOnly = true;

    if (data == nullptr || data_size <= 0) {
        if (ImGui::Begin("Hex Editor", &GuiGlobalConstants::is_hex_editor_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
            GuiGlobalConstants::ClampWindowToScreen();
            ImGui::TextWrapped("No data loaded.");
            ImGui::TextWrapped("Select any file from the DAT browser to view its raw bytes here.");
        }
        ImGui::End();
    }
    else {
        mem_edit.DrawWindow("Hex Editor", &GuiGlobalConstants::is_hex_editor_open, data, data_size);
    }
}
