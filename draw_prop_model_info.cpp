#include "pch.h"
#include "draw_prop_model_info.h"

void draw_prop_model_info(const FFNA_ModelFile& model)
{
    ImGui::Text("FFNA Signature: %.4s", model.ffna_signature);
    ImGui::Text("FFNA Type: %d", model.ffna_type);

    if (ImGui::TreeNode("Geometry Chunk"))
    {
        const GeometryChunk& geometry_chunk = model.geometry_chunk;
        ImGui::Text("Chunk ID: %u", geometry_chunk.chunk_id);
        ImGui::Text("Chunk Size: %u", geometry_chunk.chunk_size);

        if (ImGui::TreeNode("Chunk1_sub1"))
        {
            const Chunk1_sub1& sub_1 = geometry_chunk.sub_1;

            ImGui::Text("some_type_maybe: %u", sub_1.some_type_maybe);
            ImGui::Text("f0x4: %u", sub_1.f0x4);
            ImGui::Text("f0x8: %u", sub_1.f0x8);
            ImGui::Text("f0xC: %u", sub_1.f0xC);
            ImGui::Text("f0x10: %u", sub_1.f0x10);
            ImGui::Text("f0x14: %u", sub_1.f0x14);
            ImGui::Text("f0x15: %u", sub_1.f0x15);
            ImGui::Text("f0x16: %u", sub_1.f0x16);
            ImGui::Text("f0x17: %u", sub_1.f0x17);
            ImGui::Text("some_num0: %u", sub_1.some_num0);
            ImGui::Text("some_num1: %u", sub_1.some_num1);
            ImGui::Text("f0x20: %u", sub_1.f0x20);

            ImGui::Text("f0x24:");
            ImGui::Indent();
            for (int i = 0; i < 8; ++i)
            {
                ImGui::Text("f0x24[%d]: %u", i, sub_1.f0x24[i]);
            }
            ImGui::Unindent();

            ImGui::Text("f0x2C: %u", sub_1.f0x2C);
            ImGui::Text("num_some_struct0: %u", sub_1.num_some_struct0);

            ImGui::Text("f0x31:");
            ImGui::Indent();
            for (int i = 0; i < 7; ++i)
            {
                ImGui::Text("f0x31[%d]: %u", i, sub_1.f0x31[i]);
            }
            ImGui::Unindent();

            ImGui::Text("f0x38: %u", sub_1.f0x38);
            ImGui::Text("f0x3C: %u", sub_1.f0x3C);
            ImGui::Text("f0x40: %u", sub_1.f0x40);
            ImGui::Text("num_models: %u", sub_1.num_models);
            ImGui::Text("f0x48: %u", sub_1.f0x48);
            ImGui::Text("f0x4C: %hu", sub_1.f0x4C);

            ImGui::Text("f0x4E:");
            ImGui::Indent();
            for (int i = 0; i < 2; ++i)
            {
                ImGui::Text("f0x4E[%d]: %u", i, sub_1.f0x4E[i]);
            }
            ImGui::Unindent();

            ImGui::Text("f0x50: %hu", sub_1.f0x50);
            ImGui::Text("num_some_struct2: %hu", sub_1.num_some_struct2);

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Unknown"))
        {
            for (size_t i = 0; i < geometry_chunk.unknown.size(); ++i)
            {
                ImGui::Text("unknown[%zu]: %u", i, geometry_chunk.unknown[i]);
            }
            ImGui::TreePop();
        }
        for (int j = 0; j < geometry_chunk.models.size(); j++)
        {
            auto sub_model = geometry_chunk.models[j];
            auto tree_name = std::format("Sub model - {}", j);
            if (ImGui::TreeNode(tree_name.c_str()))
            {

                ImGui::Text("Unknown: %u", sub_model.unknown);
                ImGui::Text("Num Indices 0 : %u", sub_model.num_indices0);
                ImGui::Text("Num Indices 1: %u", sub_model.num_indices1);
                ImGui::Text("Num Indices 2: %u", sub_model.num_indices2);
                ImGui::Text("Total num indices: %u", sub_model.total_num_indices);
                ImGui::Text("Num vertices: %u", sub_model.num_vertices);

                const auto dat_fvf_hash_text =
                  std::format("0x{:X} ({})", sub_model.dat_fvf, sub_model.dat_fvf);
                ImGui::Text("DAT FVF: %s", dat_fvf_hash_text.c_str());

                ImGui::Text("u0: %u", sub_model.num_indices0);
                ImGui::Text("u1: %u", sub_model.num_indices1);
                ImGui::Text("u2: copy 2: %u", sub_model.num_indices2);

                if (ImGui::TreeNode("Indices"))
                {
                    for (size_t i = 0; i < sub_model.indices.size(); ++i)
                    {
                        ImGui::Text("indices[%zu]: %hu", i, sub_model.indices[i]);
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Vertices"))
                {
                    for (size_t i = 0; i < sub_model.vertices.size(); ++i)
                    {
                        const ModelVertex& vertex = sub_model.vertices[i];
                        if (ImGui::TreeNodeEx((void*)(intptr_t)i, ImGuiTreeNodeFlags_DefaultOpen,
                                              "vertices[%zu]", i))
                        {
                            ImGui::Text("x: %.3f, y: %.3f, z: %.3f", vertex.x, vertex.y, vertex.z);
                            if (ImGui::TreeNode("Dunno Data"))
                            {
                                for (size_t j = 0; j < vertex.dunno.size(); ++j)
                                {
                                    ImGui::Text("dunno[%zu]: %.3f", j, vertex.dunno[j]);
                                }
                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Extra data"))
                {
                    for (size_t i = 0; i < sub_model.extra_data.size(); ++i)
                    {
                        ImGui::Text("extra_data[%zu]: %u", i, sub_model.extra_data[i]);
                    }
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }
        }

        if (ImGui::TreeNode("Chunk Data"))
        {
            for (size_t i = 0; i < geometry_chunk.chunk_data.size(); ++i)
            {
                ImGui::Text("chunk_data[%zu]: %u", i, geometry_chunk.chunk_data[i]);
            }
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}
