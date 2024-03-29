#include "pch.h"
#include "draw_props_info_panel.h"


void draw_props_info_panel(const Chunk3& chunk)
{
    if (ImGui::TreeNode("Props info"))
    {
        ImGui::Text("Chunk ID: %u", chunk.chunk_id);
        ImGui::Text("Chunk Size: %u", chunk.chunk_size);
        ImGui::Text("Magic Number: %u", chunk.magic_number);
        ImGui::Text("Magic Number2: %hu", chunk.magic_number2);
        ImGui::Text("Prop Array Size in Bytes: %u", chunk.prop_array_size_in_bytes);

        if (ImGui::TreeNode("Prop Array"))
        {
            ImGui::Text("Number of Props: %hu", chunk.prop_array.num_props);

            for (size_t i = 0; i < chunk.prop_array.props_info.size(); i++)
            {
                const PropInfo& prop = chunk.prop_array.props_info[i];

                if (ImGui::TreeNode((std::string("PropInfo #") + std::to_string(i)).c_str()))
                {
                    draw_prop_info(prop);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Some Vertex Data"))
        {
            ImGui::Text("f0: %hhu", chunk.some_vertex_data.f0);
            ImGui::Text("Array Size in Bytes: %u", chunk.some_vertex_data.array_size_in_bytes);
            ImGui::Text("Number of Elements: %u", chunk.some_vertex_data.num_elements);

            for (size_t i = 0; i < chunk.some_vertex_data.vertices.size(); i++)
            {
                const SomeVertex& vertex = chunk.some_vertex_data.vertices[i];

                if (ImGui::TreeNode((std::string("SomeVertex #") + std::to_string(i)).c_str()))
                {
                    ImGui::Text("Position: (%f, %f, %f)", vertex.x, vertex.y, vertex.z);
                    ImGui::Text("f6: %u", vertex.f6);
                    ImGui::Text("f7: %u", vertex.f7);
                    ImGui::Text("f8: %u", vertex.f8);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Some Data"))
        {
            ImGui::Text("f0: %hhu", chunk.some_data0.f0);
            ImGui::Text("Array Size in Bytes: %u", chunk.some_data0.array_size_in_bytes);
            ImGui::Text("Number of Elements: %u", chunk.some_data0.num_elements);

            for (size_t i = 0; i < chunk.some_data0.array.size(); i++)
            {
                ImGui::Text("Element #%zu: %hu", i, chunk.some_data0.array[i]);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Some Data1"))
        {
            ImGui::Text("f0: %hhu", chunk.some_data1.f0);
            ImGui::Text("Array Size in Bytes: %u", chunk.some_data1.array_size_in_bytes);
            ImGui::Text("Number of Elements: %u", chunk.some_data1.num_elements);

            for (size_t i = 0; i < chunk.some_data1.array.size(); i++)
            {
                const Vertex2& vertex = chunk.some_data1.array[i];
                ImGui::Text("Vertex2 #%zu: (%f, %f)", i, vertex.x, vertex.y);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Some Data2"))
        {
            ImGui::Text("f0: %hhu", chunk.some_data2.f0);
            ImGui::Text("Array Size in Bytes: %u", chunk.some_data2.array_size_in_bytes);
            ImGui::Text("Number of Elements: %hu", chunk.some_data2.num_elements);

            for (size_t i = 0; i < chunk.some_data2.array.size(); i++)
            {
                const SomeData2Struct& sd2_struct = chunk.some_data2.array[i];

                if (ImGui::TreeNode((std::string("SomeData2Struct #") + std::to_string(i)).c_str()))
                {
                    ImGui::Text("f0: %hu", sd2_struct.f0);
                    ImGui::Text("Prop Index: %hu", sd2_struct.prop_index);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}
