#include "pch.h"
#include "draw_hex_editor_panel.h"
#include "imgui_memory_editor.h"

void draw_hex_editor_panel(unsigned char* data, int data_size)
{
    static MemoryEditor mem_edit;
    mem_edit.DrawWindow("Memory Editor", data, data_size);
}
