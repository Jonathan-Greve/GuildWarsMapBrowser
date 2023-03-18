#pragma once
#pragma once
#include <vector>
#include <DirectXMath.h>
#include "MeshInstance.h"
#include "FFNA_MapFile.h"

class Terrain
{
public:
    Terrain(int32_t grid_dim_x, uint32_t grid_dim_y, const std::vector<float>& height_map,
            const MapBounds& bounds)
        : m_gridDimX(grid_dim_x)
        , m_gridDimY(grid_dim_y)
        , m_heightMap(height_map)
        , m_bounds(bounds)
    {
        // Generate terrain mesh
        mesh = std::make_unique<Mesh>(GenerateTerrainMesh());
    }

    Mesh* get_mesh() { return mesh.get(); }

private:
    // Generates a terrain mesh based on the height map data
#include <DirectXMath.h>

    Mesh GenerateTerrainMesh()
    {
        float deltaX = (m_bounds.map_max_x - m_bounds.map_min_x) / (m_gridDimX);
        float deltaY = (m_bounds.map_max_y - m_bounds.map_min_y) / (m_gridDimY);

        uint32_t grid_dims = 32;
        uint32_t sub_grid_rows = m_gridDimY / grid_dims;
        uint32_t sub_grid_cols = m_gridDimX / grid_dims;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (int j = 0; j < sub_grid_rows - 1; j++)
        {
            float terrain_sub_grid_y_start = m_bounds.map_max_y - j * grid_dims * deltaY;
            //float terrain_sub_grid_y_start = m_bounds.map_min_y + j * grid_dims * deltaY;

            for (int i = 0; i < sub_grid_cols; i++)
            {
                int curr_sub_grid = j * sub_grid_cols + i;

                int tile_start_idx = curr_sub_grid * (grid_dims + 1) * (grid_dims + 1);

                float terrain_sub_grid_x_start = m_bounds.map_min_x + i * grid_dims * deltaX;

                for (int sub_grid_row = 0; sub_grid_row < grid_dims - 1; sub_grid_row++)
                {
                    for (int sub_grid_col = 0; sub_grid_col < grid_dims - 1; sub_grid_col++)
                    {
                        float height_tl =
                          -m_heightMap[tile_start_idx + sub_grid_row * grid_dims + sub_grid_col];
                        float height_tr =
                          -m_heightMap[tile_start_idx + sub_grid_row * grid_dims + sub_grid_col + 1];
                        float height_bl =
                          -m_heightMap[tile_start_idx + (sub_grid_row + 1) * grid_dims + sub_grid_col];
                        float height_br =
                          -m_heightMap[tile_start_idx + (sub_grid_row + 1) * grid_dims + sub_grid_col + 1];

                        DirectX::XMFLOAT3 pos_tl(terrain_sub_grid_x_start + sub_grid_col * deltaX, height_tl,
                                                 terrain_sub_grid_y_start - sub_grid_row * deltaY);
                        DirectX::XMFLOAT3 pos_tr(terrain_sub_grid_x_start + (sub_grid_col + 1) * deltaX,
                                                 height_tr, terrain_sub_grid_y_start - sub_grid_row * deltaY);
                        DirectX::XMFLOAT3 pos_bl(terrain_sub_grid_x_start + sub_grid_col * deltaX, height_bl,
                                                 terrain_sub_grid_y_start - (sub_grid_row + 1) * deltaY);
                        DirectX::XMFLOAT3 pos_br(terrain_sub_grid_x_start + (sub_grid_col + 1) * deltaX,
                                                 height_br,
                                                 terrain_sub_grid_y_start - (sub_grid_row + 1) * deltaY);

                        auto normal_tl = ComputeTriangleNormal(pos_tl, pos_tr, pos_bl);
                        auto normal_tr = ComputeTriangleNormal(pos_tr, pos_br, pos_bl);

                        auto vertex_tl = Vertex(pos_tl, normal_tl, {0, 0});
                        auto vertex_tr = Vertex(pos_tr, normal_tl, {0, 0});
                        auto vertex_bl = Vertex(pos_bl, normal_tl, {0, 0});
                        auto vertex_br = Vertex(pos_br, normal_tr, {0, 0});

                        uint32_t vertex_index = static_cast<uint32_t>(vertices.size());

                        vertices.push_back(vertex_tl);
                        vertices.push_back(vertex_tr);
                        vertices.push_back(vertex_bl);
                        vertices.push_back(vertex_br);

                        // Add indices of two clockwise triangles.
                        indices.push_back(vertex_index); // top-left
                        indices.push_back(vertex_index + 1); // top-right
                        indices.push_back(vertex_index + 2); // bottom-left

                        indices.push_back(vertex_index + 1); // top-right
                        indices.push_back(vertex_index + 3); // bottom-right
                        indices.push_back(vertex_index + 2); // bottom-left
                    }
                }
            }
        }

        return {vertices, indices};
    }

    XMFLOAT3
    ComputeTriangleNormal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
    {
        XMVECTOR vEdge1 = XMVectorSubtract(XMLoadFloat3(&v1), XMLoadFloat3(&v0));
        XMVECTOR vEdge2 = XMVectorSubtract(XMLoadFloat3(&v2), XMLoadFloat3(&v0));
        XMVECTOR vNormal = XMVector3Cross(vEdge1, vEdge2);
        XMFLOAT3 normal;
        XMStoreFloat3(&normal, XMVector3Normalize(vNormal));
        return normal;
    }

    uint32_t m_gridDimX;
    uint32_t m_gridDimY;
    std::vector<float> m_heightMap;
    MapBounds m_bounds;
    std::unique_ptr<Mesh> mesh;
};
