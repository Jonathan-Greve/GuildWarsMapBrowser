#pragma once
#include "FFNA_MapFile.h"
#include "FFNA_ModelFile.h"

inline void draw_prop_filename_element(size_t i, const Chunk4DataElement& element, bool default_open = false)
{
	auto flag = ImGuiTreeNodeFlags_None;
	if (default_open) flag = ImGuiTreeNodeFlags_DefaultOpen;
	if (ImGui::TreeNodeEx((std::string("Element #") + std::to_string(i)).c_str(), flag))
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

void draw_props_filenames_panel(const Chunk4& chunk);
