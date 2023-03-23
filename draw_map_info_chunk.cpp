#include "pch.h"
#include "draw_map_info_chunk.h"

void draw_map_info_chunk(const Chunk2& chunk)
{
    if (ImGui::TreeNode("Chunk 2"))
    {
        ImGui::Text("Chunk ID: %u", chunk.chunk_id);
        ImGui::Text("Chunk Size: %u", chunk.chunk_size);
        ImGui::Text("Magic Number: %u", chunk.magic_num);
        ImGui::Text("Always 2: %u", chunk.always_2);

        if (ImGui::TreeNode("Map Bounds"))
        {
            ImGui::Text("Map Min X: %.2f", chunk.map_bounds.map_min_x);
            ImGui::Text("Map Max X: %.2f", chunk.map_bounds.map_max_x);
            ImGui::Text("Map Min Z: %.2f", chunk.map_bounds.map_min_z);
            ImGui::Text("Map Max Z: %.2f", chunk.map_bounds.map_max_z);
            ImGui::TreePop();
        }

        ImGui::Text("F0: %u", chunk.f0);
        ImGui::Text("F1: %u", chunk.f1);
        ImGui::Text("F2: %u", chunk.f2);
        ImGui::Text("F3: %u", chunk.f3);
        ImGui::Text("F4: %u", chunk.f4);

        ImGui::TreePop();
    }
}
