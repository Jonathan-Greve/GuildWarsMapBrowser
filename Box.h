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
        float half_width = size.x * 0.5f;
        float half_height = size.y * 0.5f;
        float half_depth = size.z * 0.5f;

        std::vector<Vertex> vertices = {
          // Front face
          {{-half_width, -half_height, -half_depth}, {0, 0, -1}, {0, 1}},
          {{half_width, -half_height, -half_depth}, {0, 0, -1}, {1, 1}},
          {{half_width, half_height, -half_depth}, {0, 0, -1}, {1, 0}},
          {{-half_width, half_height, -half_depth}, {0, 0, -1}, {0, 0}},
          // Back face
          {{-half_width, -half_height, half_depth}, {0, 0, 1}, {1, 1}},
          {{half_width, -half_height, half_depth}, {0, 0, 1}, {0, 1}},
          {{half_width, half_height, half_depth}, {0, 0, 1}, {0, 0}},
          {{-half_width, half_height, half_depth}, {0, 0, 1}, {1, 0}},
          // Top face
          {{-half_width, half_height, -half_depth}, {0, 1, 0}, {0, 1}},
          {{half_width, half_height, -half_depth}, {0, 1, 0}, {1, 1}},
          {{half_width, half_height, half_depth}, {0, 1, 0}, {1, 0}},
          {{-half_width, half_height, half_depth}, {0, 1, 0}, {0, 0}},
          // Bottom face
          {{-half_width, -half_height, -half_depth}, {0, -1, 0}, {1, 1}},
          {{half_width, -half_height, -half_depth}, {0, -1, 0}, {0, 1}},
          {{half_width, -half_height, half_depth}, {0, -1, 0}, {0, 0}},
          {{-half_width, -half_height, half_depth}, {0, -1, 0}, {1, 0}},
          // Left face
          {{-half_width, -half_height, -half_depth}, {-1, 0, 0}, {1, 1}},
          {{-half_width, half_height, -half_depth}, {-1, 0, 0}, {1, 0}},
          {{-half_width, half_height, half_depth}, {-1, 0, 0}, {0, 0}},
          {{-half_width, -half_height, half_depth}, {-1, 0, 0}, {0, 1}},
          // Right face
          {{half_width, -half_height, -half_depth}, {1, 0, 0}, {0, 1}},
          {{half_width, half_height, -half_depth}, {1, 0, 0}, {0, 0}},
          {{half_width, half_height, half_depth}, {1, 0, 0}, {1, 0}},
          {{half_width, -half_height, half_depth}, {1, 0, 0}, {1, 1}},
        };

        std::vector<uint32_t> indices = {
          0,  2,  1,  0,  3,  2, // Front face
          4,  6,  5,  4,  7,  6, // Back face
          8,  10, 9,  8,  11, 10, // Top face
          12, 14, 13, 12, 15, 14, // Bottom face
          16, 18, 17, 16, 19, 18, // Left face
          20, 22, 21, 20, 23, 22, // Right face
        };

        return Mesh(vertices, indices);
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
