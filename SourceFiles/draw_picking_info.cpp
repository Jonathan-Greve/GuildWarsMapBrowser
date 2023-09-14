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

	// Display the object ID under the cursor
	if (info.object_id >= 0) { ImGui::Text("Picked Object ID: %d", info.object_id); }
	else { ImGui::Text("Picked Object ID: None"); }

	if (last_hovered_prop_index >= 0) { ImGui::Text("Picked Prop Index: %d", last_hovered_prop_index); }
	else { ImGui::Text("Picked Object ID: None"); }

	if (last_hovered_prop_index < selected_ffna_map_file.props_info_chunk.prop_array.props_info.size())
	{
		const PropInfo prop_info = selected_ffna_map_file.props_info_chunk.prop_array.props_info[
			last_hovered_prop_index];
		draw_prop_info(prop_info, true);

		if (prop_info.filename_index < selected_ffna_map_file.prop_filenames_chunk.array.size())
		{
			draw_prop_filename_element(prop_info.filename_index,
			                           selected_ffna_map_file.prop_filenames_chunk.array[prop_info.filename_index], true);
		}

		if (auto ffna_model_file_ptr =
			std::get_if<FFNA_ModelFile>(&selected_map_files[prop_info.filename_index])) { }
	}

	// End the ImGui window
	ImGui::End();
}
