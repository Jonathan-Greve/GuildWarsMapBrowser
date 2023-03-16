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
        std::vector<Vertex> vertices = {
          // Front face
          {{-size.x / 2, -size.y / 2, size.z / 2}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
          {{size.x / 2, -size.y / 2, size.z / 2}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
          {{size.x / 2, size.y / 2, size.z / 2}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
          {{-size.x / 2, size.y / 2, size.z / 2}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},

          // Back face
          {{-size.x / 2, -size.y / 2, -size.z / 2}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
          {{size.x / 2, -size.y / 2, -size.z / 2}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
          {{size.x / 2, size.y / 2, -size.z / 2}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
          {{-size.x / 2, size.y / 2, -size.z / 2}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},

          // Left face
          {{-size.x / 2, -size.y / 2, -size.z / 2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
          {{-size.x / 2, -size.y / 2, size.z / 2}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
          {{-size.x / 2, size.y / 2, size.z / 2}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
          {{-size.x / 2, size.y / 2, -size.z / 2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},

          // Right face
          {{size.x / 2, -size.y / 2, -size.z / 2}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
          {{size.x / 2, -size.y / 2, size.z / 2}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
          {{size.x / 2, size.y / 2, size.z / 2}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
          {{size.x / 2, size.y / 2, -size.z / 2}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},

          // Top face
          {{-size.x / 2, size.y / 2, -size.z / 2}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
          {{-size.x / 2, size.y / 2, size.z / 2}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
          {{size.x / 2, size.y / 2, size.z / 2}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
          {{size.x / 2, size.y / 2, -size.z / 2}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},

          // Bottom face
          {{-size.x / 2, -size.y / 2, -size.z / 2}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
          {{-size.x / 2, -size.y / 2, size.z / 2}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
          {{size.x / 2, -size.y / 2, size.z / 2}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
          {{size.x / 2, -size.y / 2, -size.z / 2}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        };

        std::vector<uint32_t> indices = {
          0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11, 12, 13,
          14, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
        };

        return Mesh(vertices, indices);
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
