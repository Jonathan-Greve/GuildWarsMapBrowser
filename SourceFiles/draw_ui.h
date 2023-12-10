#pragma once
#include "MapBrowser.h"

struct PickingInfo;
void draw_ui(std::map<int, std::unique_ptr<DATManager>>& dat_managers, int& dat_manager_to_show, MapRenderer* map_renderer, PickingInfo picking_info, std::vector<std::vector<std::string>>& csv_data);
