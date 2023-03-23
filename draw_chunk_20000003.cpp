#include "pch.h"
#include "draw_chunk_20000003.h"

void draw_chunk_20000003(const Chunk5& chunk)
{
    if (ImGui::TreeNode("Chunk5"))
    {
        ImGui::Text("Chunk ID: 0x%08X", chunk.chunk_id);
        ImGui::Text("Chunk Size: %u", chunk.chunk_size);
        ImGui::Text("Magic Num: %u", chunk.magic_num);
        ImGui::Text("Magic Num1: %u", chunk.magic_num1);

        if (ImGui::TreeNode("Element 0"))
        {
            ImGui::Text("Tag: 0x%02X", chunk.element_0.tag);
            ImGui::Text("Size: %u", chunk.element_0.size);
            if (ImGui::TreeNode("Data"))
            {
                for (size_t i = 0; i < chunk.element_0.data.size(); ++i)
                {
                    ImGui::Text("Data[%zu]: 0x%02X", i, chunk.element_0.data[i]);
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Element 1"))
        {
            ImGui::Text("Tag: 0x%02X", chunk.element_1.tag);
            ImGui::Text("Size: %u", chunk.element_1.size);
            if (ImGui::TreeNode("Data"))
            {
                for (size_t i = 0; i < chunk.element_1.data.size(); ++i)
                {
                    ImGui::Text("Data[%zu]: 0x%02X", i, chunk.element_1.data[i]);
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Element 2"))
        {
            ImGui::Text("Tag: 0x%02X", chunk.element_2.tag);
            ImGui::Text("Size: %u", chunk.element_2.size);
            ImGui::Text("Num Zones: %u", chunk.element_2.num_zones);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Element 3"))
        {
            ImGui::Text("Tag: 0x%02X", chunk.element_3.tag);
            ImGui::Text("Size: %u", chunk.element_3.size);
            if (ImGui::TreeNode("Data"))
            {
                for (size_t i = 0; i < chunk.element_3.data.size(); ++i)
                {
                    ImGui::Text("Data[%zu]: 0x%02X", i, chunk.element_3.data[i]);
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }

        if (chunk.element_2.num_zones > 0)
        {
            ImGui::Text("Unknown0: %u", chunk.unknown0);
            ImGui::Text("Unknown1: %u", chunk.unknown1);

            if (ImGui::TreeNode("Unknown2"))
            {
                for (size_t i = 0; i < chunk.unknown2.size(); ++i)
                {
                    ImGui::Text("Unknown2[%zu]: %f", i, chunk.unknown2[i]);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Some Array"))
            {
                for (size_t i = 0; i < chunk.some_array.size(); ++i)
                {
                    ImGui::Text("Chunk5Element2[%zu]", i);
                    // Display the contents of chunk.some_array[i] if needed
                }
                ImGui::TreePop();
            }
        }

        if (! chunk.chunk_data.empty())
        {
            ImGui::Text("Chunk Data Size: %zu", chunk.chunk_data.size());
            // Display the chunk_data if needed
        }

        ImGui::TreePop();
    }
}
