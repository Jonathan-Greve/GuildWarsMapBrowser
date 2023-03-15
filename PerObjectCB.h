#pragma once
#include <DirectXMath.h>

// Per-object constant buffer struct
struct PerObjectCB
{
    DirectX::XMFLOAT4X4 world;
    // Add any other per-object data here
};
