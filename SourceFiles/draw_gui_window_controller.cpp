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
			GuiGlobalConstants::is_pathfinding_panel_open))
	{
		GuiGlobalConstants::hide_all = false;
	}
}

void draw_gui_window_controller()
{
	// Begin a new ImGui window
	ImGui::Begin("Window Controller", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

	// Checkbox to hide or show all windows
	CheckAndResetHideAll();
	if (ImGui::Checkbox("Hide All", &GuiGlobalConstants::hide_all))
	{
		if (GuiGlobalConstants::hide_all)
		{
			GuiGlobalConstants::SaveCurrentStates();

			GuiGlobalConstants::is_dat_browser_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_dat_browser_movable = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_dat_browser_resizeable = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_dat_browser_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_left_panel_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_right_panel_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_hex_editor_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_text_panel_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_audio_controller_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_texture_panel_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_picking_panel_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_compare_panel_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_custom_file_info_editor_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_extract_panel_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_byte_search_panel_open = !GuiGlobalConstants::hide_all;
			GuiGlobalConstants::is_pathfinding_panel_open = !GuiGlobalConstants::hide_all;
		}
		else
		{
			GuiGlobalConstants::RestorePreviousStates();
		}
	}

	// Checkboxes for individual window control
	ImGui::Checkbox("DAT Browser", &GuiGlobalConstants::is_dat_browser_open);
	// Add additional controls for properties like resizable, movable etc. if needed
	ImGui::Checkbox("Left Panel", &GuiGlobalConstants::is_left_panel_open);
	ImGui::Checkbox("Right Panel", &GuiGlobalConstants::is_right_panel_open);
	ImGui::Checkbox("Hex Editor", &GuiGlobalConstants::is_hex_editor_open);
	ImGui::Checkbox("Text Panel", &GuiGlobalConstants::is_text_panel_open);
	ImGui::Checkbox("Audio Controller", &GuiGlobalConstants::is_audio_controller_open);
	ImGui::Checkbox("Texture Panel", &GuiGlobalConstants::is_texture_panel_open);
	ImGui::Checkbox("Picking Panel", &GuiGlobalConstants::is_picking_panel_open);
	ImGui::Checkbox("Compare Panel", &GuiGlobalConstants::is_compare_panel_open);
	ImGui::Checkbox("Custom File Info Editor", &GuiGlobalConstants::is_custom_file_info_editor_open);
	ImGui::Checkbox("Extract Panel", &GuiGlobalConstants::is_extract_panel_open);
	ImGui::Checkbox("Byte Pattern Search Panel", &GuiGlobalConstants::is_byte_search_panel_open);
	ImGui::Checkbox("Pathfinding Panel", &GuiGlobalConstants::is_pathfinding_panel_open);

	ImGui::Separator();
	if (ImGui::Checkbox("DAT Browser movable and resizeable", &GuiGlobalConstants::is_dat_browser_movable)) {
		GuiGlobalConstants::is_dat_browser_resizeable = GuiGlobalConstants::is_dat_browser_movable;
	}


	// End the ImGui window
	ImGui::End();
}
