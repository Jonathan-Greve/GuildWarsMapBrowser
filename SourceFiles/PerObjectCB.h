#pragma once
#include <DirectXMath.h>

struct PerObjectCB
{
    DirectX::XMFLOAT4X4 world;
    int num_textures;
    float pad[3];

    // Constructor to set the default values
    PerObjectCB()
    {
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());
        num_textures = 0;
    }
};
