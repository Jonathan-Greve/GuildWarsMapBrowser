#include "pch.h"
#include "draw_terrain_chunk.h"

void draw_terrain_chunk(const Chunk8& chunk)
{

    if (ImGui::TreeNode("Terrain chunk"))
    {
        ImGui::Text("Chunk ID: 0x%08X", chunk.chunk_id);
        ImGui::Text("Chunk Size: %u", chunk.chunk_size);
        ImGui::Text("Magic Num: 0x%08X", chunk.magic_num);
        ImGui::Text("Magic Num1: 0x%08X", chunk.magic_num1);
        ImGui::Text("Tag: 0x%02X", chunk.tag);
        ImGui::Text("Some Size: %u", chunk.some_size);
        ImGui::Text("Terrain X Dims: %u", chunk.terrain_x_dims);
        ImGui::Text("Terrain Y Dims: %u", chunk.terrain_y_dims);
        ImGui::Text("Some Float: %f", chunk.some_float);
        ImGui::Text("Some Small Float: %f", chunk.some_small_float);
        ImGui::Text("Some Size1: %u", chunk.some_size1);
        ImGui::Text("Some Float1: %f", chunk.some_float1);
        ImGui::Text("Some Float2: %f", chunk.some_float2);
        ImGui::Text("Tag1: 0x%02X", chunk.tag1);
        ImGui::Text("Terrain Height Size Bytes: %u", chunk.terrain_height_size_bytes);

        if (ImGui::TreeNode("Terrain Heightmap"))
        {
            for (size_t i = 0; i < chunk.terrain_heightmap.size(); ++i)
            {
                ImGui::Text("Heightmap[%zu]: %f", i, chunk.terrain_heightmap[i]);
            }
            ImGui::TreePop();
        }

        ImGui::Text("Tag2: 0x%02X", chunk.tag2);
        ImGui::Text("Num Terrain Tiles: %u", chunk.num_terrain_tiles);

        if (ImGui::TreeNode("Terrain texture indices (maybe)"))
        {
            for (size_t i = 0; i < chunk.terrain_texture_indices_maybe.size(); ++i)
            {
                ImGui::Text("Tile[%zu]: 0x%02X", i, chunk.terrain_texture_indices_maybe[i]);
            }
            ImGui::TreePop();
        }

        ImGui::Text("Tag3: 0x%02X", chunk.tag3);
        ImGui::Text("Some Size2: %u", chunk.some_size2);
        ImGui::Text("Some Size3: %u", chunk.some_size3);

        if (ImGui::TreeNode("Some Data3"))
        {
            for (size_t i = 0; i < chunk.some_data3.size(); ++i)
            {
                ImGui::Text("Data3[%zu]: 0x%02X", i, chunk.some_data3[i]);
            }
            ImGui::TreePop();
        }

        ImGui::Text("Tag4: 0x%02X", chunk.tag4);
        ImGui::Text("Some Size4: %u", chunk.some_size4);

        if (ImGui::TreeNode("Some Data4"))
        {
            for (size_t i = 0; i < chunk.some_data4.size(); ++i)
            {
                ImGui::Text("Data4[%zu]: 0x%02X", i, chunk.some_data4[i]);
            }
            ImGui::TreePop();
        }

        ImGui::Text("Tag5: 0x%02X", chunk.tag5);
        ImGui::Text("Some Size5: %u", chunk.some_size5);

        if (ImGui::TreeNode("Some Data5"))
        {
            for (size_t i = 0; i < chunk.some_data5.size(); ++i)
            {
                ImGui::Text("Data5[%zu]: 0x%02X", i, chunk.some_data5[i]);
            }
            ImGui::TreePop();
        }

        ImGui::Text("Tag6: 0x%02X", chunk.tag6);
        ImGui::Text("Num Terrain Tiles1: %u", chunk.num_terrain_tiles1);

        if (ImGui::TreeNode("Something For Each Tile1"))
        {
            for (size_t i = 0; i < chunk.terrain_texture_blend_weights_maybe.size(); ++i)
            {
                ImGui::Text("Tile1[%zu]: 0x%02X", i, chunk.terrain_texture_blend_weights_maybe[i]);
            }
            ImGui::TreePop();
        }

        ImGui::Text("Tag7: 0x%02X", chunk.tag7);
        ImGui::Text("Some Size7: %u", chunk.some_size7);

        if (ImGui::TreeNode("Some Data7"))
        {
            for (size_t i = 0; i < chunk.some_data7.size(); ++i)
            {
                ImGui::Text("Data7[%zu]: 0x%02X", i, chunk.some_data7[i]);
            }
            ImGui::TreePop();
        }

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
