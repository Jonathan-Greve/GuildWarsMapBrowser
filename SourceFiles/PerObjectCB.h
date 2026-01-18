#pragma once

struct PerObjectCB
{
    DirectX::XMFLOAT4X4 world;
    uint32_t uv_indices[2][4];
    uint32_t texture_indices[2][4];
    uint32_t blend_flags[2][4];
    uint32_t texture_types[2][4];
    uint32_t num_uv_texture_pairs;
    uint32_t object_id;
    uint32_t highlight_state;
    float shore_max_alpha;
    float shore_wave_speed;
    float mesh_alpha;  // Alpha multiplier for mesh transparency (0.0 to 1.0)
    float pad[2];  // Padding to align object_color to 16-byte boundary (HLSL requires float4 alignment)
    DirectX::XMFLOAT4 object_color;  // Solid color for debug primitives (used when num_uv_texture_pairs == 0)

    PerObjectCB()
        : num_uv_texture_pairs(0), object_id(0), highlight_state(0), shore_max_alpha(0.0f), shore_wave_speed(0.0f)
        , mesh_alpha(1.0f), pad{0.0f, 0.0f}, object_color(1.0f, 1.0f, 1.0f, 1.0f)  // Default to white
    {
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());

        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                uv_indices[i][j] = 0;
                texture_indices[i][j] = 0;
                blend_flags[i][j] = 0;
                texture_types[i][j] = 0;
            }
        }
    }
};
