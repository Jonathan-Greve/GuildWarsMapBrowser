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

        uint32_t grid_dims = 32;
        uint32_t sub_grid_rows = m_gridDimY / grid_dims;
        uint32_t sub_grid_cols = m_gridDimX / grid_dims;

        std::vector<std::vector<float>> grid(m_gridDimY + 1, std::vector<float>(m_gridDimX + 1, 0.0f));

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
                        grid[m_gridDimY - k][l] = -m_heightMap[count];
                        count++;
                    }
                }
            }
        }

        float deltaX = (m_bounds.map_max_x - m_bounds.map_min_x) / (m_gridDimX);
        float deltaY = (m_bounds.map_max_y - m_bounds.map_min_y) / (m_gridDimY);

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Loop over the grid and generate the vertices and normals using ComputeTriangleNormal
        for (uint32_t y = 0; y < m_gridDimY; y++)
        {
            for (uint32_t x = 0; x < m_gridDimX; x++)
            {
                float xPos = m_bounds.map_min_x + x * deltaX;
                float yPos = grid[y][x];
                float zPos = m_bounds.map_min_y + y * deltaY;

                vertices.push_back(
                  {{xPos, yPos, zPos}, {0.0f, 0.0f, 0.0f}, {(float)x / m_gridDimX, (float)y / m_gridDimY}});

                if (x < m_gridDimX - 1 && y < m_gridDimY - 1)
                {
                    uint32_t currentIndex = y * m_gridDimX + x;
                    indices.push_back(currentIndex);
                    indices.push_back(currentIndex + m_gridDimX + 1);
                    indices.push_back(currentIndex + 1);

                    indices.push_back(currentIndex);
                    indices.push_back(currentIndex + m_gridDimX);
                    indices.push_back(currentIndex + m_gridDimX + 1);
                }
            }
        }

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            Vertex& v0 = vertices[indices[i]];
            Vertex& v1 = vertices[indices[i + 1]];
            Vertex& v2 = vertices[indices[i + 2]];

            XMFLOAT3 normal = ComputeTriangleNormal(v0.position, v1.position, v2.position);
            v0.normal = normal;
            v1.normal = normal;
            v2.normal = normal;
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
