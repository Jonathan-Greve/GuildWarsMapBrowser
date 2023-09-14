#include "pch.h"
#include "draw_props_filenames_panel.h"


void draw_props_filenames_panel(const Chunk4& chunk)
{

    if (ImGui::TreeNode("Props filenames chunk"))
    {
        ImGui::Text("Chunk ID: 0x%08X", chunk.chunk_id);
        ImGui::Text("Chunk Size: %u", chunk.chunk_size);

        if (ImGui::TreeNode("Data Header"))
        {
            ImGui::Text("Signature: 0x%08X", chunk.data_header.signature);
            ImGui::Text("Version: %hhu", chunk.data_header.version);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Filenames array"))
        {
            ImGui::Text("Number of Elements: %zu", chunk.array.size());

            for (size_t i = 0; i < chunk.array.size(); i++)
            {
                const Chunk4DataElement& element = chunk.array[i];

                draw_prop_filename_element(i, element);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Chunk Data"))
        {
            ImGui::Text("Chunk Data Size: %zu", chunk.chunk_data.size());

            ImGui::BeginChild("ChunkData", ImVec2(0, 200), true);
            for (size_t i = 0; i < chunk.chunk_data.size(); i++)
            {
                ImGui::Text("%02X ", chunk.chunk_data[i]);
                if (i % 16 == 15)
                    ImGui::NewLine();
            }
            ImGui::EndChild();
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
}
