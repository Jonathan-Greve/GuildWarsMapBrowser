#include "pch.h"
#include "draw_picking_info.h"

#include "draw_dat_browser.h"
#include "FFNA_MapFile.h"

extern FFNA_MapFile selected_ffna_map_file;
extern std::vector<FileData> selected_map_files;

void draw_picking_info(const PickingInfo& info)
{
	// Create a new ImGui window called "Picking Info"
	ImGui::Begin("Picking Info");

	// Display mouse coordinates
	ImGui::Text("Mouse Coordinates: (%d, %d)", info.client_x, info.client_y);

	// Display the object ID under the cursor
	if (info.object_id >= 0) { ImGui::Text("Picked Object ID: %d", info.object_id); }
	else { ImGui::Text("Picked Object ID: None"); }

	if (info.prop_index >= 0) { ImGui::Text("Picked Prop Index: %d", info.prop_index); }
	else { ImGui::Text("Picked Object ID: None"); }

	if (info.prop_index < selected_ffna_map_file.props_info_chunk.prop_array.props_info.size())
	{
		PropInfo prop_info = selected_ffna_map_file.props_info_chunk.prop_array.props_info[info.prop_index];
		if (auto ffna_model_file_ptr =
			std::get_if<FFNA_ModelFile>(&selected_map_files[prop_info.filename_index]))
		{
			ImGui::Text("Num models: %d", ffna_model_file_ptr->geometry_chunk.models.size());
		}
	}

	// End the ImGui window
	ImGui::End();
}
