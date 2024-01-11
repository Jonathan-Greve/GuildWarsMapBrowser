#pragma once
struct ExtractPanelInfo {
    int pixels_per_tile_x = 1;
    int pixels_per_tile_y = 1;
    bool pixels_per_tile_changed = false;
    std::wstring save_directory = L"";
};

void draw_extract_panel(ExtractPanelInfo& extract_panel_info);