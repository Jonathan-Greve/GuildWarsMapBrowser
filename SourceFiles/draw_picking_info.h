#pragma once
#include "MapRenderer.h"
#include "DATManager.h"

struct PickingInfo
{
    int client_x;
    int client_y;
    int object_id;
    int prop_index;
    int prop_submodel_index;
    DirectX::XMFLOAT3 camera_pos;
};

void draw_picking_info(const PickingInfo& info, MapRenderer* map_renderer, DATManager* dat_manager, std::unordered_map<int, std::vector<int>>& hash_index);