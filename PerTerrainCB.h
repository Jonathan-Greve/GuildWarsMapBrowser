#pragma once
struct PerTerrainCB
{
    int grid_dim_x;
    int grid_dim_y;
    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float min_z;
    float max_z;
    float water_level;
    float pad[3];

    uint8_t terrain_texture_indices[65536 * 4];
};
