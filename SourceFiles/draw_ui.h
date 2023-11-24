#pragma once
#include "MapBrowser.h"

struct PickingInfo;
void draw_ui(std::map<int, std::unique_ptr<DATManager>>& dat_managers, MapRenderer* map_renderer, PickingInfo picking_info);
