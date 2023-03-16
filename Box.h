#pragma once
#include "MeshInstance.h"

class Box : public MeshInstance
{
public:
    Box(ID3D11Device* device, const DirectX::XMFLOAT3& size, int id)
        : MeshInstance(device, GenerateBoxMesh(size), id)
    {
    }

private:
    Mesh GenerateBoxMesh(const DirectX::XMFLOAT3& size)
    {
        float half_width = size.x / 2.0f;
        float half_height = size.y / 2.0f;
        float half_depth = size.z / 2.0f;

        std::vector<Vertex> vertices = {
          {{-half_width, -half_height, half_depth}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
          {{-half_width, half_height, half_depth}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
          {{half_width, half_height, half_depth}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
          {{half_width, -half_height, half_depth}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
          {{-half_width, -half_height, -half_depth}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
          {{-half_width, half_height, -half_depth}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
          {{half_width, half_height, -half_depth}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
          {{half_width, -half_height, -half_depth}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        };

        std::vector<uint32_t> indices = {
          // Front face
          0,
          1,
          2,
          0,
          2,
          3,

          // Back face
          4,
          6,
          5,
          4,
          7,
          6,

          // Left face
          4,
          5,
          1,
          4,
          1,
          0,

          // Right face
          3,
          2,
          6,
          3,
          6,
          7,

          // Top face
          1,
          5,
          6,
          1,
          6,
          2,

          // Bottom face
          4,
          0,
          3,
          4,
          3,
          7,
        };

        return Mesh(vertices, indices);
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
