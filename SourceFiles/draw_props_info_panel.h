#pragma once
#include "FFNA_MapFile.h"

void draw_props_info_panel(const Chunk3& chunk);

inline void draw_prop_info(const PropInfo& prop, bool default_open = false)
{
	ImGui::Text("model filename index: %hu", prop.filename_index);
	ImGui::Text("Position: (%f, %f, %f)", prop.x, prop.y, prop.z);
	ImGui::Text("f4: %f", prop.f4);
	ImGui::Text("f5: %f", prop.f5);
	ImGui::Text("f6: %f", prop.f6);
	ImGui::Text("sin_angle: %f", prop.sin_angle);
	ImGui::Text("cos_angle: %f", prop.cos_angle);
	ImGui::Text("f9: %f", prop.f9);
	ImGui::Text("scaling_factor: %f", prop.scaling_factor);
	ImGui::Text("f11: %f", prop.f11);
	ImGui::Text("f12: %hhu", prop.f12);
	ImGui::Text("num_some_structs: %hhu", prop.num_some_structs);

	auto flag = ImGuiTreeNodeFlags_None;
	if (default_open) flag = ImGuiTreeNodeFlags_DefaultOpen;
	if (ImGui::TreeNodeEx("Some Structs", flag))
	{
		for (size_t j = 0; j < prop.some_structs.size(); j += 8)
		{
			ImGui::Text("Some Struct #%zu", j / 8);
			ImGui::Text("Data: ");
			for (int k = 0; k < 8; ++k)
			{
				ImGui::SameLine();
				ImGui::Text("%02X", prop.some_structs[j + k]);
			}
		}
		ImGui::TreePop();
	}
}
