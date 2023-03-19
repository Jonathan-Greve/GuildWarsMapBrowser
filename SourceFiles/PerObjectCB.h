#pragma once
#include <DirectXMath.h>

struct PerObjectCB
{
    DirectX::XMFLOAT4X4 world;
    // Add any other per-object data here

    // Constructor to set the default values
    PerObjectCB() { DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity()); }
};
