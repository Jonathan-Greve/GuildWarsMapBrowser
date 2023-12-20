#include "pch.h"
#include "draw_hex_editor_panel.h"
#include "imgui_memory_editor.h"
#include "GuiGlobalConstants.h"
void draw_hex_editor_panel(unsigned char* data, int data_size)
{
    static MemoryEditor mem_edit;
    mem_edit.ReadOnly = true;

    if (GuiGlobalConstants::is_hex_editor_open) {
        mem_edit.DrawWindow("Memory Editor", &GuiGlobalConstants::is_hex_editor_open, data, data_size);
    }
}
