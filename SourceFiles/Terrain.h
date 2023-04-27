#pragma once
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
            const std::vector<uint8_t>& terrain_texture_blend_weights, const MapBounds& bounds)
        : m_grid_dim_x(grid_dim_x)
        , m_grid_dim_z(grid_dim_y)
        , m_height_map(height_map)
        , m_terrain_texture_indices(terrain_texture_indices)
        , m_terrain_texture_blend_weights(terrain_texture_blend_weights)
        , m_bounds(bounds)
    {
        // Generate terrain mesh
        mesh = std::make_unique<Mesh>(GenerateTerrainMesh());
    }

    Mesh* get_mesh() { return mesh.get(); }

    uint32_t m_grid_dim_x;
    uint32_t m_grid_dim_z;
    MapBounds m_bounds;
    PerTerrainCB m_per_terrain_cb;
    std::vector<std::vector<uint32_t>> m_texture_index_grid;
    std::vector<std::vector<uint32_t>> m_texture_blend_weights_grid;

    void update_per_terrain_CB(PerTerrainCB& new_cb) { m_per_terrain_cb = new_cb; }
    const std::vector<std::vector<uint32_t>>& get_texture_index_grid() const { return m_texture_index_grid; }
    const std::vector<std::vector<uint32_t>>& get_texture_blend_weights_grid() const
    {
        return m_texture_blend_weights_grid;
    }

private:
    // Generates a terrain mesh based on the height map data
#include <DirectXMath.h>

    Mesh GenerateTerrainMesh()
    {

        uint32_t grid_dims = 32;
        uint32_t sub_grid_rows = m_grid_dim_z / grid_dims;
        uint32_t sub_grid_cols = m_grid_dim_x / grid_dims;

        std::vector<std::vector<float>> grid(m_grid_dim_z + 1, std::vector<float>(m_grid_dim_x + 1, 0.0f));

        // Each element is the index into the texture atlas representing which texture to use on that tile.
        m_texture_index_grid.resize(m_grid_dim_z + 1, std::vector<uint32_t>(m_grid_dim_x + 1, 0.0f));

        m_texture_blend_weights_grid.resize(m_grid_dim_z + 1, std::vector<uint32_t>(m_grid_dim_x + 1, 0.0f));

        float min = FLT_MAX;
        float max = FLT_MIN;

        int count = 0;
        for (int j = 0; j < sub_grid_rows; j++) // rows
        {
            for (int i = 0; i < sub_grid_cols; i++) // cols
            {
                // The positions in the grid we are inserting into
                int col_start = i * grid_dims;
                int col_end = col_start + grid_dims;
                int row_start = j * grid_dims;
                int row_end = row_start + grid_dims;

                for (int k = row_start; k < row_end; k++)
                {
                    for (int l = col_start; l < col_end; l++)
                    {
                        grid[m_grid_dim_z - k][l] = -m_height_map[count];
                        m_texture_index_grid[k > 0 ? k - 1 : 0][l] = m_terrain_texture_indices[count];
                        m_texture_blend_weights_grid[k > 0 ? k - 1 : 0][l] =
                          m_terrain_texture_blend_weights[count];
                        count++;

                        if (grid[m_grid_dim_z - k][l] < min)
                        {
                            min = grid[m_grid_dim_z - k][l];
                        }
                        if (grid[m_grid_dim_z - k][l] > max)
                        {
                            max = grid[m_grid_dim_z - k][l];
                        }
                    }
                }
            }
        }

        m_bounds.map_max_y = max;
        m_bounds.map_min_y = min;

        float delta_x = (m_bounds.map_max_x - m_bounds.map_min_x) / (m_grid_dim_x);
        float delta_z = (m_bounds.map_max_z - m_bounds.map_min_z) / (m_grid_dim_z);

        std::vector<GWVertex> vertices;
        std::vector<uint32_t> indices;

        // Loop over the grid and generate the vertices and normals using ComputeTriangleNormal
        for (uint32_t z = 0; z < m_grid_dim_z; z++)
        {
            for (uint32_t x = 0; x < m_grid_dim_x; x++)
            {
                float xPos = m_bounds.map_min_x + x * delta_x;
                float yPos = grid[z][x];
                float zPos = m_bounds.map_min_z + z * delta_z;

                vertices.push_back({{xPos, yPos, zPos},
                                    {0.0f, 0.0f, 0.0f},
                                    {(float)x / (m_grid_dim_x - 1), 1.0f - (float)z / (m_grid_dim_z - 1)},
                                    {0.0f, 0.0f},
                                    {0.0f, 0.0f},
                                    {0.0f, 0.0f},
                                    {0.0f, 0.0f},
                                    {0.0f, 0.0f},
                                    {0.0f, 0.0f},
                                    {0.0f, 0.0f}});

                if (x < m_grid_dim_x - 1 && z < m_grid_dim_z - 1)
                {
                    uint32_t currentIndex = z * m_grid_dim_x + x;
                    indices.push_back(currentIndex);
                    indices.push_back(currentIndex + m_grid_dim_x + 1);
                    indices.push_back(currentIndex + 1);

                    indices.push_back(currentIndex);
                    indices.push_back(currentIndex + m_grid_dim_x);
                    indices.push_back(currentIndex + m_grid_dim_x + 1);
                }
            }
        }

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            GWVertex& v0 = vertices[indices[i]];
            GWVertex& v1 = vertices[indices[i + 1]];
            GWVertex& v2 = vertices[indices[i + 2]];

            XMFLOAT3 normal = compute_normal(v0.position, v1.position, v2.position);
            v0.normal = normal;
            v1.normal = normal;
            v2.normal = normal;
        }

        m_per_terrain_cb =
          PerTerrainCB(m_grid_dim_x, m_grid_dim_z, m_bounds.map_min_x, m_bounds.map_max_x, m_bounds.map_min_y,
                       m_bounds.map_max_y, m_bounds.map_min_z, m_bounds.map_max_z, 0, {0, 0, 0});

        return Mesh(vertices, indices, {0}, {0}, {0}, true, BlendState::Opaque, 1);
    }

    std::vector<float> m_height_map;
    std::vector<uint8_t> m_terrain_texture_indices;
    std::vector<uint8_t> m_terrain_texture_blend_weights;
    std::unique_ptr<Mesh> mesh;
};
