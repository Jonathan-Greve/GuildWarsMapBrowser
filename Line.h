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
        // Generate line mesh here
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
