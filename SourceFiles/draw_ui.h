#pragma once
#include "MapBrowser.h"

struct PickingInfo;
void draw_ui(std::map<int, std::unique_ptr<DATManager>>& dat_managers, int& dat_manager_to_show, MapRenderer* map_renderer, PickingInfo picking_info, std::vector<std::vector<std::string>>& csv_data, 
    int& FPS_target, DX::StepTimer& timer, ExtractPanelInfo& extract_panel_info, bool& msaa_changed, int& msaa_level_index, const std::vector<std::pair<int, int>>& msaa_levels);
