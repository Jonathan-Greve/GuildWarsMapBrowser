#include "pch.h"
#include "draw_picking_info.h"

void draw_picking_info(const PickingInfo& info)
{
    // Create a new ImGui window called "Picking Info"
    ImGui::Begin("Picking Info");

    // Display mouse coordinates
    ImGui::Text("Mouse Coordinates: (%d, %d)", info.client_x, info.client_y);

    // Display the object ID under the cursor
    if(info.object_id >= 0)
    {
        ImGui::Text("Picked Object ID: %d", info.object_id);
    }
    else
    {
        ImGui::Text("Picked Object ID: None");
    }

    if(info.prop_index >= 0)
    {
        ImGui::Text("Picked Prop Index: %d", info.prop_index);
    }
    else
    {
        ImGui::Text("Picked Object ID: None");
    }

    // End the ImGui window
    ImGui::End();
}
