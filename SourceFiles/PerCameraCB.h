#pragma once
#include <DirectXMath.h>

// Per-camera constant buffer struct
struct PerCameraCB
{
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT4X4 directional_light_view;
    DirectX::XMFLOAT4X4 directional_light_proj;
    DirectX::XMFLOAT4X4 reflection_view;
    DirectX::XMFLOAT4X4 reflection_proj;
    DirectX::XMFLOAT3 position;
    float pad0;
    float shadowmap_texel_size_x;
    float shadowmap_texel_size_y;
    float pad1[2];
    float reflection_texel_size_x;
    float reflection_texel_size_y;
    float pad2[2];
};
