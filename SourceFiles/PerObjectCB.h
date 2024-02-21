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
    float pad[1];

    PerObjectCB()
        : num_uv_texture_pairs(0), object_id(0), highlight_state(0)
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
