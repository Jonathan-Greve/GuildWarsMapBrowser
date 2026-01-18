#include "pch.h"
#include "draw_gui_window_controller.h"
#include "GuiGlobalConstants.h"
#include "imgui.h"

void CheckAndResetHideAll()
{
	// If 'hide_all' is true but any window is manually opened, reset 'hide_all' to false
	if (GuiGlobalConstants::hide_all &&
		(GuiGlobalConstants::is_dat_browser_open ||
			GuiGlobalConstants::is_left_panel_open ||
			GuiGlobalConstants::is_right_panel_open ||
			GuiGlobalConstants::is_hex_editor_open ||
			GuiGlobalConstants::is_text_panel_open ||
			GuiGlobalConstants::is_audio_controller_open ||
			GuiGlobalConstants::is_texture_panel_open ||
			GuiGlobalConstants::is_picking_panel_open ||
			GuiGlobalConstants::is_compare_panel_open ||
			GuiGlobalConstants::is_custom_file_info_editor_open ||
			GuiGlobalConstants::is_extract_panel_open ||
			GuiGlobalConstants::is_byte_search_panel_open ||
			GuiGlobalConstants::is_pathfinding_panel_open ||
			GuiGlobalConstants::is_animation_panel_open))
	{
		GuiGlobalConstants::hide_all = false;
	}
}

void draw_gui_window_controller()
{
	// Track if window was open before Begin() to detect X button click
	bool was_open = GuiGlobalConstants::is_window_controller_open;

	// Begin a new ImGui window with close button
	if (!ImGui::Begin("Window Controller", &GuiGlobalConstants::is_window_controller_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
		ImGui::End();
		// Save settings if closed via X button
		if (was_open && !GuiGlobalConstants::is_window_controller_open) {
			GuiGlobalConstants::SaveSettings();
		}
		return;
	}

	// Check if X button was clicked (window still visible but flag changed)
	if (was_open && !GuiGlobalConstants::is_window_controller_open) {
		GuiGlobalConstants::SaveSettings();
	}

	// Checkbox to hide or show all windows
	CheckAndResetHideAll();
	bool temp_hide_all = GuiGlobalConstants::hide_all;
	if (ImGui::Checkbox("Hide All", &temp_hide_all))
	{
		GuiGlobalConstants::SetHideAll(temp_hide_all);
		GuiGlobalConstants::SaveSettings();
	}

	// Checkboxes for individual window control
	bool changed = false;
	changed |= ImGui::Checkbox("DAT Browser", &GuiGlobalConstants::is_dat_browser_open);
	// Add additional controls for properties like resizable, movable etc. if needed
	changed |= ImGui::Checkbox("Left Panel", &GuiGlobalConstants::is_left_panel_open);
	changed |= ImGui::Checkbox("Right Panel", &GuiGlobalConstants::is_right_panel_open);
	changed |= ImGui::Checkbox("Hex Editor", &GuiGlobalConstants::is_hex_editor_open);
	changed |= ImGui::Checkbox("Text Panel", &GuiGlobalConstants::is_text_panel_open);
	changed |= ImGui::Checkbox("Audio Controller", &GuiGlobalConstants::is_audio_controller_open);
	changed |= ImGui::Checkbox("Texture Panel", &GuiGlobalConstants::is_texture_panel_open);
	changed |= ImGui::Checkbox("Picking Panel", &GuiGlobalConstants::is_picking_panel_open);
	changed |= ImGui::Checkbox("Compare Panel", &GuiGlobalConstants::is_compare_panel_open);
	changed |= ImGui::Checkbox("Custom File Info Editor", &GuiGlobalConstants::is_custom_file_info_editor_open);
	changed |= ImGui::Checkbox("Extract Panel", &GuiGlobalConstants::is_extract_panel_open);
	changed |= ImGui::Checkbox("Byte Pattern Search Panel", &GuiGlobalConstants::is_byte_search_panel_open);
	changed |= ImGui::Checkbox("Pathfinding Panel", &GuiGlobalConstants::is_pathfinding_panel_open);
	changed |= ImGui::Checkbox("Animation Controller", &GuiGlobalConstants::is_animation_panel_open);
	if (changed) GuiGlobalConstants::SaveSettings();

	ImGui::Separator();
	if (ImGui::Checkbox("DAT Browser movable and resizeable", &GuiGlobalConstants::is_dat_browser_movable)) {
		GuiGlobalConstants::is_dat_browser_resizeable = GuiGlobalConstants::is_dat_browser_movable;
		GuiGlobalConstants::SaveSettings();
	}

	ImGui::Separator();
	if (ImGui::Button("Reset to Defaults", ImVec2(-FLT_MIN, 0))) {
		GuiGlobalConstants::ResetToDefaults();
		GuiGlobalConstants::SaveSettings();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Reset all window visibility to default state");
	}

	// End the ImGui window
	ImGui::End();
}
