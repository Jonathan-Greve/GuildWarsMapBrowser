#include "pch.h"
#include "Terrain.h"

float Terrain::get_height_at(float world_x, float world_z) const
{
    // Convert world coordinates to grid coordinates
    float grid_x = (world_x - m_bounds.map_min_x) / (m_bounds.map_max_x - m_bounds.map_min_x) * m_grid_dim_x;
    float grid_z = (world_z - m_bounds.map_min_z) / (m_bounds.map_max_z - m_bounds.map_min_z) * m_grid_dim_z;

    // Get the indices of the bottom left corner of the cell containing the point
    int cell_x = static_cast<int>(grid_x);
    int cell_z = static_cast<int>(grid_z);

    // Check for edge cases where the point lies exactly on the maximum bounds
    if (cell_x >= m_grid_dim_x - 1) cell_x = m_grid_dim_x - 2;
    if (cell_z >= m_grid_dim_z - 1) cell_z = m_grid_dim_z - 2;

    // Calculate the relative positions within the cell (between 0 and 1)
    float dx = grid_x - cell_x;
    float dz = grid_z - cell_z;

    // Retrieve the heights at the corners of the cell
    float h00 = grid[cell_z][cell_x];     // Bottom left
    float h10 = grid[cell_z][cell_x + 1]; // Bottom right
    float h01 = grid[cell_z + 1][cell_x]; // Top left
    float h11 = grid[cell_z + 1][cell_x + 1]; // Top right

    // Perform bilinear interpolation
    float height = h00 * (1 - dx) * (1 - dz) +
        h10 * dx * (1 - dz) +
        h01 * (1 - dx) * dz +
        h11 * dx * dz;

    return height;
}
