#include "pch.h"
#include "draw_picking_info.h"

#include "draw_dat_browser.h"
#include "draw_props_filenames_panel.h"
#include "draw_props_info_panel.h"
#include "FFNA_MapFile.h"

extern FFNA_MapFile selected_ffna_map_file;
extern std::vector<FileData> selected_map_files;

void draw_picking_info(const PickingInfo& info)
{
	static int last_hovered_prop_index = -1;
	if (info.prop_index >= 0) { last_hovered_prop_index = info.prop_index; }

	// Create a new ImGui window called "Picking Info"
	ImGui::Begin("Picking Info");

	// Display mouse coordinates
	ImGui::Text("Mouse Coordinates: (%d, %d)", info.client_x, info.client_y);


	if (last_hovered_prop_index >= 0) { ImGui::Text("Picked Prop Index: %d", last_hovered_prop_index); }
	else { ImGui::Text("Picked Object ID: None"); }

	if (last_hovered_prop_index < selected_ffna_map_file.props_info_chunk.prop_array.props_info.size())
	{
		const PropInfo prop_info = selected_ffna_map_file.props_info_chunk.prop_array.props_info[
			last_hovered_prop_index];
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
			for (size_t i = 0; i < models.size(); ++i)
			{
				const auto& model = models[i];

				if (ImGui::TreeNodeEx(("Model " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					const auto uts1 = ffna_model_file_ptr->geometry_chunk.uts1;
					if (uts1.size() > 0)
					{
						ImGui::Text("some_flags0: %u", uts1[i % uts1.size()].some_flags0);
						ImGui::Text("f0x6: %u", uts1[i % uts1.size()].f0x6);
						ImGui::Text("f0x8: %u", uts1[i % uts1.size()].f0x8);
					}

					ImGui::Text("Num vertices: %u", model.num_vertices);
					ImGui::Text("Num indices: %u", model.total_num_indices);
					ImGui::Text("Min X: %f, Max X: %f", model.minX, model.maxX);
					ImGui::Text("Min Y: %f, Max Y: %f", model.minY, model.maxY);
					ImGui::Text("Min Z: %f, Max Z: %f", model.minZ, model.maxZ);

					if (model.vertices.size() > 0)
					{
						const auto& first_vertex = model.vertices[0];

						if (ImGui::TreeNodeEx("First Vertex Info", ImGuiTreeNodeFlags_DefaultOpen))
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

							ImGui::TreePop();
						}
					}

					ImGui::TreePop();
				}
			}
		}
	}

	// End the ImGui window
	ImGui::End();
}
