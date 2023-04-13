struct PerObjectCB
{
    DirectX::XMFLOAT4X4 world;
    uint32_t uv_indices[8][4];
    uint32_t texture_indices[8][4];
    uint32_t blend_flags[8][4];
    uint32_t num_uv_texture_pairs;
    float pad[3];

    PerObjectCB()
        : num_uv_texture_pairs(0)
    {
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());

        for (int i = 0; i < 8; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                uv_indices[i][j] = 0;
                texture_indices[i][j] = 0;
                blend_flags[i][j] = 0;
            }
        }
    }
};
