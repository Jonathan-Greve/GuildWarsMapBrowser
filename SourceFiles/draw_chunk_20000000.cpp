#include "pch.h"
#include "draw_chunk_20000000.h"

void draw_chunk_20000000(const Chunk1& chunk)
{
    if (ImGui::TreeNode("Chunk 1"))
    {
        ImGui::Text("Chunk ID: %u", chunk.chunk_id);
        ImGui::Text("Chunk Size: %u", chunk.chunk_size);

        if (ImGui::TreeNode("Chunk Data"))
        {
            for (size_t i = 0; i < chunk.chunk_data.size(); ++i)
            {
                ImGui::Text("Chunk Data[%zu]: 0x%02X", i, chunk.chunk_data[i]);
            }
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}
