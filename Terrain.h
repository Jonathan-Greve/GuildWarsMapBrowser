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
    Mesh GenerateTerrainMesh()
    {
        Mesh terrainMesh;
        float deltaX = (m_bounds.map_max_x - m_bounds.map_min_x) / (m_gridDimX - 1);
        float deltaY = (m_bounds.map_max_y - m_bounds.map_min_y) / (m_gridDimY - 1);

        // Generate vertices, indices, and normals per tile
        for (uint32_t y = 0; y < m_gridDimY - 1; ++y)
        {
            for (uint32_t x = 0; x < m_gridDimX - 1; ++x)
            {
                // Get height data for tile corners
                float heights[4] = {m_heightMap[y * m_gridDimX + x], m_heightMap[y * m_gridDimX + x + 1],
                                    m_heightMap[(y + 1) * m_gridDimX + x],
                                    m_heightMap[(y + 1) * m_gridDimX + x + 1]};

                // Calculate positions for tile corners
                XMFLOAT3 positions[4] = {
                  XMFLOAT3(m_bounds.map_min_x + x * deltaX, -heights[0], m_bounds.map_min_y + y * deltaY),
                  XMFLOAT3(m_bounds.map_min_x + (x + 1) * deltaX, -heights[1],
                           m_bounds.map_min_y + y * deltaY),
                  XMFLOAT3(m_bounds.map_min_x + x * deltaX, -heights[2],
                           m_bounds.map_min_y + (y + 1) * deltaY),
                  XMFLOAT3(m_bounds.map_min_x + (x + 1) * deltaX, -heights[3],
                           m_bounds.map_min_y + (y + 1) * deltaY)};

                // Calculate texture coordinates for tile corners
                XMFLOAT2 tex_coords[4] = {XMFLOAT2(static_cast<float>(x) / (m_gridDimX - 1),
                                                   static_cast<float>(y) / (m_gridDimY - 1)),
                                          XMFLOAT2(static_cast<float>(x + 1) / (m_gridDimX - 1),
                                                   static_cast<float>(y) / (m_gridDimY - 1)),
                                          XMFLOAT2(static_cast<float>(x) / (m_gridDimX - 1),
                                                   static_cast<float>(y + 1) / (m_gridDimY - 1)),
                                          XMFLOAT2(static_cast<float>(x + 1) / (m_gridDimX - 1),
                                                   static_cast<float>(y + 1) / (m_gridDimY - 1))};

                // Calculate normals for the two triangles
                XMFLOAT3 normal1 = ComputeTriangleNormal(positions[0], positions[2], positions[3]);
                XMFLOAT3 normal2 = ComputeTriangleNormal(positions[0], positions[3], positions[1]);

                // Triangle 1
                terrainMesh.vertices.push_back({positions[0], normal1, tex_coords[0]});
                terrainMesh.vertices.push_back({positions[2], normal1, tex_coords[2]});
                terrainMesh.vertices.push_back({positions[3], normal1, tex_coords[3]});
                terrainMesh.indices.push_back(terrainMesh.vertices.size() - 3);
                terrainMesh.indices.push_back(terrainMesh.vertices.size() - 2);
                terrainMesh.indices.push_back(terrainMesh.vertices.size() - 1);

                // Triangle 2
                terrainMesh.vertices.push_back({positions[0], normal2, tex_coords[0]});
                terrainMesh.vertices.push_back({positions[3], normal2, tex_coords[3]});
                terrainMesh.vertices.push_back({positions[1], normal2, tex_coords[1]});
                terrainMesh.indices.push_back(terrainMesh.vertices.size() - 3);
                terrainMesh.indices.push_back(terrainMesh.vertices.size() - 2);
                terrainMesh.indices.push_back(terrainMesh.vertices.size() - 1);
            }
        }

        return terrainMesh;
    }

    XMFLOAT3 ComputeTriangleNormal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
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
