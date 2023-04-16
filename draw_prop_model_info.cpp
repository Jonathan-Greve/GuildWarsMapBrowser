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
            ImGui::Text("max_UV_index: %u", sub_1.max_UV_index);
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
            ImGui::Text("collision_count: %hu", sub_1.collision_count);

            ImGui::Text("f0x4E:");
            ImGui::Indent();
            for (int i = 0; i < 2; ++i)
            {
                ImGui::Text("f0x4E[%d]: %u", i, sub_1.f0x4E[i]);
            }
            ImGui::Unindent();

            ImGui::Text("num_some_struct2: %hu", sub_1.num_some_struct2);
            ImGui::Text("f0x52: %hu", sub_1.f0x52);

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("TextureAndVertexShader"))
        {
            const auto& texture_and_vertex_shader = geometry_chunk.tex_and_vertex_shader_struct;

            for (size_t i = 0; i < texture_and_vertex_shader.uts0.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("UnknownTexStruct0"))
                {
                    const UnknownTexStruct0& uts0 = texture_and_vertex_shader.uts0[i];
                    ImGui::Text("uts0[%zu].using_no_cull: %u", i, uts0.using_no_cull);
                    ImGui::Text("uts0[%zu].f0x1: %u", i, uts0.f0x1);
                    ImGui::Text("uts0[%zu].f0x2: %u", i, uts0.f0x2);
                    ImGui::Text("uts0[%zu].pixel_shader_id: %u", i, uts0.pixel_shader_id);
                    ImGui::Text("uts0[%zu].f0x7: %u", i, uts0.f0x7);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (ImGui::TreeNode("flags0"))
            {
                for (size_t i = 0; i < texture_and_vertex_shader.flags0.size(); ++i)
                {
                    ImGui::Text("flags0[%zu]: %u", i, texture_and_vertex_shader.flags0[i]);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("tex_array"))
            {
                for (size_t i = 0; i < texture_and_vertex_shader.tex_array.size(); ++i)
                {
                    ImGui::Text("tex_array[%zu]: %u", i, texture_and_vertex_shader.tex_array[i]);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("zeros"))
            {
                for (size_t i = 0; i < texture_and_vertex_shader.zeros.size(); ++i)
                {
                    ImGui::Text("zeros[%zu]: %u", i, texture_and_vertex_shader.zeros[i]);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("blend_state"))
            {
                for (size_t i = 0; i < texture_and_vertex_shader.blend_state.size(); ++i)
                {
                    ImGui::Text("blend_state[%zu]: %u", i, texture_and_vertex_shader.blend_state[i]);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("texture_index_UV_mapping_maybe"))
            {
                for (size_t i = 0; i < texture_and_vertex_shader.texture_index_UV_mapping_maybe.size(); ++i)
                {
                    ImGui::Text("texture_index_UV_mapping_maybe[%zu]: %u", i,
                                texture_and_vertex_shader.texture_index_UV_mapping_maybe[i]);
                }
                ImGui::TreePop();
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
                            ImGui::Text("Position: %s", vertex.has_position ? "Yes" : "No");
                            if (vertex.has_position)
                            {
                                ImGui::Text("XYZ: %.2f, %.2f, %.2f", vertex.x, vertex.y, vertex.z);
                            }

                            ImGui::Text("Group: %s", vertex.has_group ? "Yes" : "No");
                            if (vertex.has_group)
                            {
                                ImGui::Text("Group: %u", vertex.group);
                            }

                            ImGui::Text("Normal: %s", vertex.has_normal ? "Yes" : "No");
                            if (vertex.has_normal)
                            {
                                ImGui::Text("Normal: %.2f, %.2f, %.2f", vertex.normal_x, vertex.normal_y,
                                            vertex.normal_z);
                            }

                            ImGui::Text("Diffuse: %s", vertex.has_diffuse ? "Yes" : "No");
                            if (vertex.has_diffuse)
                            {
                                ImGui::Text("Diffuse: %.2f, %.2f, %.2f, %.2f", vertex.diffuse[0],
                                            vertex.diffuse[1], vertex.diffuse[2], vertex.diffuse[3]);
                            }

                            ImGui::Text("Specular: %s", vertex.has_specular ? "Yes" : "No");
                            if (vertex.has_specular)
                            {
                                ImGui::Text("Specular: %.2f, %.2f, %.2f, %.2f", vertex.specular[0],
                                            vertex.specular[1], vertex.specular[2], vertex.specular[3]);
                            }

                            ImGui::Text("Tangent: %s", vertex.has_tangent ? "Yes" : "No");
                            if (vertex.has_tangent)
                            {
                                ImGui::Text("Tangent: %.2f, %.2f, %.2f", vertex.tangent_x, vertex.tangent_y,
                                            vertex.tangent_z);
                            }

                            ImGui::Text("Bitangent: %s", vertex.has_bitangent ? "Yes" : "No");
                            if (vertex.has_bitangent)
                            {
                                ImGui::Text("Bitangent: %.2f, %.2f, %.2f", vertex.bitangent_x,
                                            vertex.bitangent_y, vertex.bitangent_z);
                            }

                            for (int j = 0; j < 8; ++j)
                            {
                                if (vertex.has_tex_coord[j])
                                {
                                    ImGui::Text("Texture Coordinate %d: Yes", j);
                                    ImGui::Text("TexCoord: %.2f, %.2f", vertex.tex_coord[j][0],
                                                vertex.tex_coord[j][1]);
                                }
                                else
                                {
                                    ImGui::Text("Texture Coordinate %d: No", j);
                                }
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
