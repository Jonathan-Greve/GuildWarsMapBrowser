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
        // Generate box mesh here
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
