#pragma once

struct PickingInfo
{
    int client_x;
    int client_y;
    int object_id;
    int prop_index;
};

void draw_picking_info(const PickingInfo& info);
