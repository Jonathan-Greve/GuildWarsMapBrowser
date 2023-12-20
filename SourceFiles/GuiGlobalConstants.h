#pragma once
class GuiGlobalConstants
{
public:
    // Some ImGui layout vars:
    inline static const int left_panel_width = 450;
    inline static const int right_panel_width = 450;
    inline static const float panel_padding = 6.0f;

    inline static bool hide_all = false;
    inline static bool is_dat_browser_open = true;
    inline static bool is_dat_browser_resizeable = true;
    inline static bool is_dat_browser_movable = true;
    inline static bool is_left_panel_open = true;
    inline static bool is_right_panel_open = true;
    inline static bool is_hex_editor_open = true;
    inline static bool is_text_panel_open = true;
    inline static bool is_audio_controller_open = true;
    inline static bool is_texture_panel_open = true;
    inline static bool is_picking_panel_open = true;
    inline static bool is_compare_panel_open = true;
    inline static bool is_custom_file_info_editor_open = true;

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
    }
};
