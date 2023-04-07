#pragma once
#include "MeshInstance.h"

class Line : public MeshInstance
{
public:
    Line(ID3D11Device* device, const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, int id)
        : MeshInstance(device, GenerateLineMesh(start, end), id)
    {
    }

private:
    Mesh GenerateLineMesh(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
    {
        std::vector<GWVertex> vertices = {
          {start,
           {0.0f, 0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f}},
          {end,
           {0.0f, 0.0f, 0.0f},
           {1.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f},
           {0.0f, 0.0f}},
        };
        std::vector<uint32_t> indices = {0, 1};

        return Mesh(vertices, indices);
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
