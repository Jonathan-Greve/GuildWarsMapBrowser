#include "pch.h"
#include "draw_picking_info.h"

#include "draw_dat_browser.h"
#include "draw_props_filenames_panel.h"
#include "draw_props_info_panel.h"
#include "FFNA_MapFile.h"
#include <algorithm>
#include <GuiGlobalConstants.h>
#include <model_exporter.h>


extern FFNA_MapFile selected_ffna_map_file;
extern std::vector<FileData> selected_map_files;

void HightlightProp(MapRenderer* map_renderer, int selected_prop_index, int selected_prop_submodel_index);
void RemoveHighlightFromProp(MapRenderer* map_renderer, int selected_prop_index);

void draw_picking_info(const PickingInfo& info, MapRenderer* map_renderer, DATManager* dat_manager, std::unordered_map<int, std::vector<int>>& hash_index)
{
    static int selected_prop_index = -1;
    static int selected_prop_submodel_index = -1;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
        RemoveHighlightFromProp(map_renderer, selected_prop_index);

        if (selected_prop_index == info.prop_index && selected_prop_submodel_index == info.prop_submodel_index) {
            selected_prop_index = -1;
            selected_prop_submodel_index = -1;
        }
        else {
            selected_prop_index = info.prop_index;
            selected_prop_submodel_index = info.prop_submodel_index;

            HightlightProp(map_renderer, selected_prop_index, selected_prop_submodel_index);
        }
    }

    const auto& props_mesh_ids = map_renderer->GetPropsMeshIds();
    const auto prop_mesh_ids_it = props_mesh_ids.find(selected_prop_index);

    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyAlt) { // Hide all submodels if shift, ctrl or alt is held down.
            if (prop_mesh_ids_it != props_mesh_ids.end()) {
                for (const auto& mesh_id : prop_mesh_ids_it->second) {
                    map_renderer->GetMeshManager()->SetMeshShouldRender(mesh_id, false);
                }
            }
        }
        else {
            if (prop_mesh_ids_it != props_mesh_ids.end()) {
                if (selected_prop_submodel_index < prop_mesh_ids_it->second.size()) {
                    const auto mesh_id = prop_mesh_ids_it->second[selected_prop_submodel_index];
                    map_renderer->GetMeshManager()->SetMeshShouldRender(mesh_id, false);
                }
            }
        }
    }

    static int last_hovered_prop_index = -1;
    static int last_hovered_prop_submodel_index = -1;
    if (info.prop_index >= 0) {
        last_hovered_prop_index = info.prop_index;
        last_hovered_prop_submodel_index = info.prop_submodel_index;
    }

    if (GuiGlobalConstants::is_picking_panel_open) {
        if (ImGui::Begin("Picking Info", &GuiGlobalConstants::is_picking_panel_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
            if (selected_prop_index >= 0 && ImGui::Button("Deselect")) {
                RemoveHighlightFromProp(map_renderer, selected_prop_index);
                selected_prop_index = -1;
                selected_prop_submodel_index = -1;
            }
            else {
                ImGui::Text("Left click a prop (3D model) to lock the selection.");
                ImGui::Text("When selected left click the object again to deselect.");
            }
            ImGui::Separator();

            int prop_index = last_hovered_prop_index;
            int submodel_index = last_hovered_prop_submodel_index;
            if (selected_prop_index >= 0) {
                prop_index = selected_prop_index;
                submodel_index = selected_prop_submodel_index;
            }

            ImGui::Text("Mouse Coordinates: (%d, %d)", info.client_x, info.client_y);

            if (prop_index >= 0) {
                ImGui::Text("Picked Prop Index: %d", prop_index);
                ImGui::Text("Submodel index: %d", submodel_index);
            }
            else { ImGui::Text("Picked Object ID: None"); }

            if (prop_index >= 0 && prop_index < selected_ffna_map_file.props_info_chunk.prop_array.props_info.size())
            {
                int prop_index = last_hovered_prop_index;
                if (selected_prop_index >= 0) {
                    prop_index = selected_prop_index;
                }

                const PropInfo prop_info = selected_ffna_map_file.props_info_chunk.prop_array.props_info[prop_index];
                draw_prop_info(prop_info, true);


                XMFLOAT3 prop_pos(prop_info.x, prop_info.y, prop_info.z);
                const auto distance = XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat3(&prop_pos),
                    XMLoadFloat3(&info.camera_pos))));
                ImGui::Text("Distance to prop: %f", distance);

                if (prop_info.filename_index < selected_ffna_map_file.prop_filenames_chunk.array.size())
                {
                    ImGui::Separator();
                    draw_prop_filename_element(prop_info.filename_index,
                        selected_ffna_map_file.prop_filenames_chunk.array[prop_info.filename_index],
                        true);
                }


                if (auto ffna_model_file_ptr =
                    std::get_if<FFNA_ModelFile>(&selected_map_files[prop_info.filename_index]))
                {
                    ImGui::Separator();
                    const std::vector<GeometryModel> models = ffna_model_file_ptr->geometry_chunk.models;
                    ImGui::Text("Num models: %d", models.size());

                    // Show info per sub-model:
                    int tex_index = 0;
                    for (size_t i = 0; i < models.size(); ++i)
                    {
                        const auto& model = models[i];

                        auto flags = ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

                        std::string treeNodeLabel = "Model " + std::to_string(i);

                        if (ImGui::TreeNodeEx(treeNodeLabel.c_str(), flags))
                        {
                            const auto uts1 = ffna_model_file_ptr->geometry_chunk.uts1;
                            if (uts1.size() > 0)
                            {
                                ImGui::Text("some_flags0: %u", uts1[i % uts1.size()].some_flags0);
                                ImGui::Text("num_textures_to_use: %u", uts1[i % uts1.size()].num_textures_to_use);
                                ImGui::Text("f0x8: %u", uts1[i % uts1.size()].f0x8);
                            }

                            ImGui::Text("Num vertices: %u", model.num_vertices);
                            ImGui::Text("Num indices: %u", model.total_num_indices);
                            ImGui::Text(".dat-FVF: %u", model.dat_fvf);
                            ImGui::Text("Min X: %f, Max X: %f", model.minX, model.maxX);
                            ImGui::Text("Min Y: %f, Max Y: %f", model.minY, model.maxY);
                            ImGui::Text("Min Z: %f, Max Z: %f", model.minZ, model.maxZ);

                            if (model.vertices.size() > 0)
                            {
                                const auto& first_vertex = model.vertices[0];

                                if (ImGui::TreeNodeEx("First Vertex Info", ImGuiTreeNodeFlags_None))
                                {
                                    ImGui::Text("Has Position: %s", first_vertex.has_position ? "True" : "False");
                                    ImGui::Text("Has Group: %s", first_vertex.has_group ? "True" : "False");
                                    ImGui::Text("Has Normal: %s", first_vertex.has_normal ? "True" : "False");
                                    ImGui::Text("Has Diffuse: %s", first_vertex.has_diffuse ? "True" : "False");
                                    ImGui::Text("Has Specular: %s", first_vertex.has_specular ? "True" : "False");
                                    ImGui::Text("Has Tangent: %s", first_vertex.has_tangent ? "True" : "False");
                                    ImGui::Text("Has Bitangent: %s", first_vertex.has_bitangent ? "True" : "False");

                                    for (int j = 0; j < 8; ++j)
                                    {
                                        ImGui::Text("Has Tex Coord %d: %s", j, first_vertex.has_tex_coord[j] ? "True" : "False");
                                    }

                                    const auto& uts0 = ffna_model_file_ptr->geometry_chunk.tex_and_vertex_shader_struct.uts0;
                                    if (model.unknown >= uts0.size()) { ImGui::Text("model index: %d (%d) >= uts0.size(): %d", i, model.unknown, uts0.size()); }

                                    if (uts0.size() > 0)
                                    {
                                        const auto uts0_j = uts0[model.unknown % uts0.size()];

                                        ImGui::Text("uts0.f0: %d", uts0_j.using_no_cull);
                                        ImGui::Text("uts0.f1: %d", uts0_j.f0x1);
                                        ImGui::Text("uts0.f2: %d", uts0_j.f0x2);
                                        ImGui::Text("uts0.f6: %d", uts0_j.pixel_shader_id);
                                        ImGui::Text("uts0.f7 (num textures): %d", uts0_j.f0x7);

                                        const auto& blend_states = ffna_model_file_ptr->geometry_chunk.
                                            tex_and_vertex_shader_struct.blend_state;
                                        for (int j = tex_index; j < std::min(tex_index + (int)uts0_j.f0x7, (int)blend_states.size()); ++j)
                                        {
                                            ImGui::Text("Blend flag %d: %d", j, blend_states[j]);
                                        }

                                        const auto& texture_flags = ffna_model_file_ptr->geometry_chunk.
                                            tex_and_vertex_shader_struct.flags0;
                                        for (int j = tex_index; j < std::min(tex_index + (int)uts0_j.f0x7, (int)texture_flags.size()); ++j)
                                        {
                                            ImGui::Text("Tex flag %d: %d", j, texture_flags[j]);
                                        }

                                        tex_index += uts0_j.f0x7;
                                    }
                                    else
                                    {
                                        ImGui::Text("uts0.size() == 0");
                                    }


                                    ImGui::TreePop();
                                }
                            }

                            ImGui::TreePop();
                        }

                        if (selected_prop_submodel_index >= 0) {
                            ImGui::SameLine();
                            if (selected_prop_submodel_index != i && ImGui::Button(("Set focus##" + std::to_string(i)).c_str()))
                            {
                                RemoveHighlightFromProp(map_renderer, selected_prop_index);
                                selected_prop_submodel_index = i;
                                HightlightProp(map_renderer, selected_prop_index, selected_prop_submodel_index);
                            }
                            else if (selected_prop_submodel_index == i && ImGui::Button(("Deselect##" + std::to_string(i)).c_str())) {
                                RemoveHighlightFromProp(map_renderer, selected_prop_index);
                                selected_prop_index = -1;
                                selected_prop_submodel_index = -1;
                            }

                            if (prop_mesh_ids_it != props_mesh_ids.end()) {
                                if (i < prop_mesh_ids_it->second.size()) {
                                    ImGui::SameLine();

                                    const auto mesh_id = prop_mesh_ids_it->second[i];
                                    bool should_render = map_renderer->GetMeshManager()->GetMeshShouldRender(mesh_id);

                                    if (should_render && ImGui::Button(("Hide##" + std::to_string(i)).c_str())) {
                                        map_renderer->GetMeshManager()->SetMeshShouldRender(mesh_id, false);
                                    }
                                    else if (!should_render && ImGui::Button(("Show##" + std::to_string(i)).c_str())) {
                                        map_renderer->GetMeshManager()->SetMeshShouldRender(mesh_id, true);
                                    }
                                }
                            }
                        }
                    }
                    if (ImGui::Button("Export model as JSON"))
                    {
                        std::wstring saveDir = OpenDirectoryDialog();
                        if (!saveDir.empty())
                        {
                            const auto filename_element = selected_ffna_map_file.prop_filenames_chunk.array[prop_info.filename_index];
                            int file_id = decode_filename(filename_element.filename.id0, filename_element.filename.id1);
                            // Use std::format to create the filename
                            std::wstring filename = std::format(L"model_0x{:X}_gwmb.json", file_id);


                            // Export the model to the chosen path
                            model_exporter::export_model(saveDir, filename, ffna_model_file_ptr, dat_manager, hash_index, map_renderer->GetTextureManager(), false);
                        }
                    }
                }
            }
        }
        ImGui::End();
    }
}

void HightlightProp(MapRenderer* map_renderer, int selected_prop_index, int selected_prop_submodel_index)
{
    const auto& props_mesh_ids = map_renderer->GetPropsMeshIds();
    const auto prop_mesh_ids_it = props_mesh_ids.find(selected_prop_index);
    if (prop_mesh_ids_it != props_mesh_ids.end()) {
        for (int i = 0; i < prop_mesh_ids_it->second.size(); i++) {
            const auto mesh_id = prop_mesh_ids_it->second[i];
            const auto object_data = map_renderer->GetMeshManager()->GetMeshPerObjectData(mesh_id);
            if (object_data.has_value()) {
                auto new_object_data = object_data.value();
                if (i == selected_prop_submodel_index) {
                    new_object_data.highlight_state = 1;
                }
                else {
                    new_object_data.highlight_state = 2;
                }

                map_renderer->GetMeshManager()->UpdateMeshPerObjectData(mesh_id, new_object_data);
            }
        }
    }
}

void RemoveHighlightFromProp(MapRenderer* map_renderer, int selected_prop_index)
{
    const auto& props_mesh_ids = map_renderer->GetPropsMeshIds();
    const auto prop_mesh_ids_it = props_mesh_ids.find(selected_prop_index);
    if (prop_mesh_ids_it != props_mesh_ids.end()) {
        for (int i = 0; i < prop_mesh_ids_it->second.size(); i++) {
            const auto mesh_id = prop_mesh_ids_it->second[i];
            const auto object_data = map_renderer->GetMeshManager()->GetMeshPerObjectData(mesh_id);
            if (object_data.has_value()) {
                auto new_object_data = object_data.value();
                new_object_data.highlight_state = 0;
                map_renderer->GetMeshManager()->UpdateMeshPerObjectData(mesh_id, new_object_data);
            }
        }
    }
}

