#pragma once
#include <vector>
#include <DirectXMath.h>
#include "MeshInstance.h"
#include "FFNA_MapFile.h"
#include "DXMathHelpers.h"
#include "PerTerrainCB.h"

class Terrain
{
public:
    Terrain(int32_t grid_dim_x, uint32_t grid_dim_y, const std::vector<float>& height_map,
            const std::vector<uint8_t>& terrain_texture_indices,
            const std::vector<uint8_t>& terrain_shadow_map, const MapBounds& bounds)
        : m_grid_dim_x(grid_dim_x)
        , m_grid_dim_z(grid_dim_y)
        , m_height_map(height_map)
        , m_terrain_texture_indices(terrain_texture_indices)
        , m_terrain_shadow_map(terrain_shadow_map)
        , m_bounds(bounds),
        grid(m_grid_dim_z + 1, std::vector<float>(m_grid_dim_x + 1, 0.0f))
    {
        // Generate terrain mesh
        mesh = std::make_unique<Mesh>(GenerateTerrainMesh());
    }

    Mesh* get_mesh() { return mesh.get(); }

    const std::vector<std::vector<float>>& get_heightmap_grid() const {
        return grid;
    }

    float get_height_at(float world_x, float world_z) const;

    uint32_t m_grid_dim_x;
    uint32_t m_grid_dim_z;
    MapBounds m_bounds;
    PerTerrainCB m_per_terrain_cb;
    std::vector<std::vector<uint32_t>> m_texture_index_grid;
    std::vector<std::vector<uint32_t>> m_terrain_shadow_map_grid;

    void update_per_terrain_CB(PerTerrainCB& new_cb) { m_per_terrain_cb = new_cb; }
    const std::vector<std::vector<uint32_t>>& get_texture_index_grid() const { return m_texture_index_grid; }
    const std::vector<std::vector<uint32_t>>& get_terrain_shadow_map_grid() const
    {
        return m_terrain_shadow_map_grid;
    }

private:
    // Generates a terrain mesh based on the height map data
    Mesh GenerateTerrainMesh();

    std::vector<float> m_height_map;
    std::vector<std::vector<float>> grid;
    std::vector<uint8_t> m_terrain_texture_indices;
    std::vector<uint8_t> m_terrain_shadow_map;
    std::unique_ptr<Mesh> mesh;
};
