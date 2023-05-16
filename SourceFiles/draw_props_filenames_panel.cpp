#include "pch.h"
#include "draw_props_filenames_panel.h"
#include "FFNA_ModelFile.h"

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

                if (ImGui::TreeNode((std::string("Element #") + std::to_string(i)).c_str()))
                {
                    ImGui::Text("f1: %hu", element.f1);
                    ImGui::Text("File Name ID0: %hu", element.filename.id0);
                    ImGui::Text("File Name ID1: %hu", element.filename.id1);

                    int decoded_hash = decode_filename(element.filename.id0, element.filename.id1);
                    const auto file_hash_text = std::format("0x{:X} ({})", decoded_hash, decoded_hash);
                    ImGui::Text("File hash: %s", file_hash_text.c_str());
                    ImGui::TreePop();
                }
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
