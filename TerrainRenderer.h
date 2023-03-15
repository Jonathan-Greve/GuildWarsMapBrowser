#pragma once
#pragma once
#include <vector>
#include <DirectXMath.h>
#include "MeshInstance.h"

class TerrainRenderer
{
public:
    TerrainRenderer(ID3D11Device* device, uint32_t grid_dim_x, uint32_t grid_dim_y,
                    const std::vector<float>& height_map, int id)
        : m_gridDimX(grid_dim_x)
        , m_gridDimY(grid_dim_y)
        , m_heightMap(height_map)
    {
        // Generate terrain mesh
        Mesh terrainMesh = GenerateTerrainMesh();

        // Create MeshInstance
        m_meshInstance = std::make_unique<MeshInstance>(device, terrainMesh, id);
    }

    void Draw(ID3D11DeviceContext* context) { m_meshInstance->Draw(context); }

private:
    // Generates a terrain mesh based on the height map data
    Mesh GenerateTerrainMesh()
    {
        Mesh terrainMesh;
        terrainMesh.vertices.resize(m_gridDimX * m_gridDimY);
        terrainMesh.indices.reserve((m_gridDimX - 1) * (m_gridDimY - 1) * 6);

        // Create vertices
        for (uint32_t y = 0; y < m_gridDimY; ++y)
        {
            for (uint32_t x = 0; x < m_gridDimX; ++x)
            {
                uint32_t index = y * m_gridDimX + x;
                float height = m_heightMap[index];
                terrainMesh.vertices[index].position = DirectX::XMFLOAT3(x, height, y);
                // Add other vertex attributes (normal, texture coordinates, etc.) if needed
            }
        }

        // Create indices
        for (uint32_t y = 0; y < m_gridDimY - 1; ++y)
        {
            for (uint32_t x = 0; x < m_gridDimX - 1; ++x)
            {
                uint32_t index = y * m_gridDimX + x;

                // First triangle
                terrainMesh.indices.push_back(index);
                terrainMesh.indices.push_back(index + m_gridDimX);
                terrainMesh.indices.push_back(index + 1);

                // Second triangle
                terrainMesh.indices.push_back(index + 1);
                terrainMesh.indices.push_back(index + m_gridDimX);
                terrainMesh.indices.push_back(index + m_gridDimX + 1);
            }
        }

        return terrainMesh;
    }

    uint32_t m_gridDimX;
    uint32_t m_gridDimY;
    std::vector<float> m_heightMap;
    std::unique_ptr<MeshInstance> m_meshInstance;
};
