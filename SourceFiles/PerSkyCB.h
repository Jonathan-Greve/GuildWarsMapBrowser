#pragma once

#include <DirectXMath.h>

// Sky parameters constant buffer (only consumed by the sky pixel shader).
struct PerSkyCB
{
    // (uv_scale, scroll_u, scroll_v, strength)
    DirectX::XMFLOAT4 cloud0_params;
    DirectX::XMFLOAT4 cloud1_params;

    // (disk_radius, strength, unused, unused)
    DirectX::XMFLOAT4 sun_params;

    // (brightness_mul, saturation_mul, brightness_bias_add, unused)
    DirectX::XMFLOAT4 color_params;
};

