#pragma once

struct PickingInfo
{
    int client_x;
    int client_y;
    int object_id;
    int prop_index;
    int prop_submodel_index;
    DirectX::XMFLOAT3 camera_pos;
};

void draw_picking_info(const PickingInfo& info);
