#pragma once
#include "imgui.h"
#include <fstream>
#include <string>
#include <filesystem>

class GuiGlobalConstants
{
public:
	inline static bool settings_loaded = false;
	// Some ImGui layout vars:
	inline static const int left_panel_width = 450;
	inline static const int right_panel_width = 450;
	inline static const float panel_padding = 6.0f;
	inline static const float menu_bar_height = 20.0f; // Height of the main menu bar

	inline static bool hide_all = false;
	inline static bool is_dat_browser_open = true;
	inline static bool is_dat_browser_resizeable = false;
	inline static bool is_dat_browser_movable = false;
	inline static bool is_left_panel_open = false;
	inline static bool is_right_panel_open = true;
	inline static bool is_hex_editor_open = false;
	inline static bool is_text_panel_open = false;
	inline static bool is_audio_controller_open = false;
	inline static bool is_texture_panel_open = false;
	inline static bool is_picking_panel_open = false;
	inline static bool is_compare_panel_open = false;
	inline static bool is_custom_file_info_editor_open = false;
	inline static bool is_extract_panel_open = false;
	inline static bool is_byte_search_panel_open = false;
	inline static bool is_pathfinding_panel_open = false;
	inline static bool is_window_controller_open = true;

	inline static bool prev_is_dat_browser_open;
	inline static bool prev_is_dat_browser_resizeable;
	inline static bool prev_is_dat_browser_movable;
	inline static bool prev_is_left_panel_open;
	inline static bool prev_is_right_panel_open;
	inline static bool prev_is_hex_editor_open;
	inline static bool prev_is_text_panel_open;
	inline static bool prev_is_audio_controller_open;
	inline static bool prev_is_texture_panel_open;
	inline static bool prev_is_picking_panel_open;
	inline static bool prev_is_compare_panel_open;
	inline static bool prev_is_custom_file_info_editor_open;
	inline static bool prev_is_extract_panel_open;
	inline static bool prev_is_byte_search_panel_open;
	inline static bool prev_is_pathfinding_panel_open;
	inline static bool prev_is_window_controller_open;

	// Method to save the current state of all panels
	static void SaveCurrentStates()
	{
		prev_is_dat_browser_open = is_dat_browser_open;
		prev_is_dat_browser_resizeable = is_dat_browser_resizeable;
		prev_is_dat_browser_movable = is_dat_browser_movable;
		prev_is_left_panel_open = is_left_panel_open;
		prev_is_right_panel_open = is_right_panel_open;
		prev_is_hex_editor_open = is_hex_editor_open;
		prev_is_text_panel_open = is_text_panel_open;
		prev_is_audio_controller_open = is_audio_controller_open;
		prev_is_texture_panel_open = is_texture_panel_open;
		prev_is_picking_panel_open = is_picking_panel_open;
		prev_is_compare_panel_open = is_compare_panel_open;
		prev_is_custom_file_info_editor_open = is_custom_file_info_editor_open;
		prev_is_extract_panel_open = is_extract_panel_open;
		prev_is_byte_search_panel_open = is_byte_search_panel_open;
		prev_is_pathfinding_panel_open = is_pathfinding_panel_open;
		prev_is_window_controller_open = is_window_controller_open;
	}

	// Method to restore the previous state of all panels
	static void RestorePreviousStates()
	{
		is_dat_browser_open = prev_is_dat_browser_open;
		is_dat_browser_movable = prev_is_dat_browser_movable;
		is_dat_browser_resizeable = prev_is_dat_browser_resizeable;
		is_left_panel_open = prev_is_left_panel_open;
		is_right_panel_open = prev_is_right_panel_open;
		is_hex_editor_open = prev_is_hex_editor_open;
		is_text_panel_open = prev_is_text_panel_open;
		is_audio_controller_open = prev_is_audio_controller_open;
		is_texture_panel_open = prev_is_texture_panel_open;
		is_picking_panel_open = prev_is_picking_panel_open;
		is_compare_panel_open = prev_is_compare_panel_open;
		is_custom_file_info_editor_open = prev_is_custom_file_info_editor_open;
		is_extract_panel_open = prev_is_extract_panel_open;
		is_byte_search_panel_open = prev_is_byte_search_panel_open;
		is_pathfinding_panel_open = prev_is_pathfinding_panel_open;
		is_window_controller_open = prev_is_window_controller_open;
	}

	// Method to reset all panels to default visibility
	static void ResetToDefaults()
	{
		hide_all = false;
		is_dat_browser_open = true;
		is_dat_browser_resizeable = false;
		is_dat_browser_movable = false;
		is_left_panel_open = false;
		is_right_panel_open = true;
		is_hex_editor_open = false;
		is_text_panel_open = false;
		is_audio_controller_open = false;
		is_texture_panel_open = false;
		is_picking_panel_open = false;
		is_compare_panel_open = false;
		is_custom_file_info_editor_open = false;
		is_extract_panel_open = false;
		is_byte_search_panel_open = false;
		is_pathfinding_panel_open = false;
		is_window_controller_open = true;
	}

	// Helper to clamp a window to stay within screen bounds
	// Call this after ImGui::Begin() returns true for floating windows
	static void ClampWindowToScreen()
	{
		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		ImVec2 display = ImGui::GetIO().DisplaySize;
		const float margin = 50.0f;

		bool needsClamp = false;

		// Ensure at least 'margin' pixels of window are visible on each side
		if (pos.x + size.x < margin) {
			pos.x = margin - size.x + 100;
			needsClamp = true;
		}
		if (pos.x > display.x - margin) {
			pos.x = display.x - margin - 100;
			needsClamp = true;
		}
		if (pos.y < 0) {
			pos.y = 10;
			needsClamp = true;
		}
		if (pos.y > display.y - margin) {
			pos.y = display.y - margin - 100;
			needsClamp = true;
		}

		if (needsClamp) {
			ImGui::SetWindowPos(pos);
		}
	}

	// Get the settings file path (next to executable)
	static std::filesystem::path GetSettingsFilePath()
	{
		wchar_t exePath[MAX_PATH];
		GetModuleFileNameW(NULL, exePath, MAX_PATH);
		std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
		return exeDir / "gui_settings.ini";
	}

	// Save window visibility settings to file
	static void SaveSettings()
	{
		std::ofstream file(GetSettingsFilePath());
		if (!file.is_open()) return;

		file << "[WindowVisibility]\n";
		file << "dat_browser=" << (is_dat_browser_open ? 1 : 0) << "\n";
		file << "dat_browser_resizeable=" << (is_dat_browser_resizeable ? 1 : 0) << "\n";
		file << "dat_browser_movable=" << (is_dat_browser_movable ? 1 : 0) << "\n";
		file << "left_panel=" << (is_left_panel_open ? 1 : 0) << "\n";
		file << "right_panel=" << (is_right_panel_open ? 1 : 0) << "\n";
		file << "hex_editor=" << (is_hex_editor_open ? 1 : 0) << "\n";
		file << "text_panel=" << (is_text_panel_open ? 1 : 0) << "\n";
		file << "audio_controller=" << (is_audio_controller_open ? 1 : 0) << "\n";
		file << "texture_panel=" << (is_texture_panel_open ? 1 : 0) << "\n";
		file << "picking_panel=" << (is_picking_panel_open ? 1 : 0) << "\n";
		file << "compare_panel=" << (is_compare_panel_open ? 1 : 0) << "\n";
		file << "custom_file_info_editor=" << (is_custom_file_info_editor_open ? 1 : 0) << "\n";
		file << "extract_panel=" << (is_extract_panel_open ? 1 : 0) << "\n";
		file << "byte_search_panel=" << (is_byte_search_panel_open ? 1 : 0) << "\n";
		file << "pathfinding_panel=" << (is_pathfinding_panel_open ? 1 : 0) << "\n";
		file << "window_controller=" << (is_window_controller_open ? 1 : 0) << "\n";

		file.close();
	}

	// Load window visibility settings from file
	static void LoadSettings()
	{
		if (settings_loaded) return;
		settings_loaded = true;

		std::ifstream file(GetSettingsFilePath());
		if (!file.is_open()) return; // Use defaults if no settings file

		std::string line;
		while (std::getline(file, line)) {
			// Skip section headers and empty lines
			if (line.empty() || line[0] == '[') continue;

			// Parse key=value
			size_t pos = line.find('=');
			if (pos == std::string::npos) continue;

			std::string key = line.substr(0, pos);
			int value = std::stoi(line.substr(pos + 1));

			if (key == "dat_browser") is_dat_browser_open = (value != 0);
			else if (key == "dat_browser_resizeable") is_dat_browser_resizeable = (value != 0);
			else if (key == "dat_browser_movable") is_dat_browser_movable = (value != 0);
			else if (key == "left_panel") is_left_panel_open = (value != 0);
			else if (key == "right_panel") is_right_panel_open = (value != 0);
			else if (key == "hex_editor") is_hex_editor_open = (value != 0);
			else if (key == "text_panel") is_text_panel_open = (value != 0);
			else if (key == "audio_controller") is_audio_controller_open = (value != 0);
			else if (key == "texture_panel") is_texture_panel_open = (value != 0);
			else if (key == "picking_panel") is_picking_panel_open = (value != 0);
			else if (key == "compare_panel") is_compare_panel_open = (value != 0);
			else if (key == "custom_file_info_editor") is_custom_file_info_editor_open = (value != 0);
			else if (key == "extract_panel") is_extract_panel_open = (value != 0);
			else if (key == "byte_search_panel") is_byte_search_panel_open = (value != 0);
			else if (key == "pathfinding_panel") is_pathfinding_panel_open = (value != 0);
			else if (key == "window_controller") is_window_controller_open = (value != 0);
		}

		file.close();
	}
};
