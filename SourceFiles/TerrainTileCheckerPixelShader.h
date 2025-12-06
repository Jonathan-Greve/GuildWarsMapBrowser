#pragma once

struct TerrainTileCheckerPixelShader
{
    static constexpr char shader_ps[] = R"(
SamplerState samLinear : register(s0);

cbuffer PerObjectCB : register(b1)
{
    matrix World;
    uint4 uv_indices[2];
    uint4 texture_indices[2];
    uint4 blend_flags[2];
    uint4 texture_types[2];
    uint num_uv_texture_pairs;
    uint object_id;
    uint highlight_state;
    float pad1[1];
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 lightingColor : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 uv3 : TEXCOORD3;
    float2 uv4 : TEXCOORD4;
    float2 uv5 : TEXCOORD5;
    float4 reflectionSpacePos : TEXCOORD6;
    float4 lightSpacePos : TEXCOORD7;
    float3 world_position : TEXCOORD8;
    float3x3 TBN : TEXCOORD9;
};

struct PSOutput
{
    float4 rt_0_output : SV_TARGET0;
    float4 rt_1_output : SV_TARGET1;
};

PSOutput main(PixelInputType input)
{
    // Each game tile is 96x96 units in world space
    const float TILE_SIZE = 96.0f;

    // Calculate which tile we're in (using floor to get tile indices)
    int tileX = (int)floor(input.world_position.x / TILE_SIZE);
    int tileZ = (int)floor(input.world_position.z / TILE_SIZE);

    // Checkerboard pattern: alternate based on sum of tile indices
    // If sum is even -> white, if odd -> black
    int checker = (tileX + tileZ) % 2;

    // Create color (0.1 for dark, 0.9 for light to avoid pure black/white)
    float intensity = (checker == 0) ? 0.9f : 0.1f;
    float4 color = float4(intensity, intensity, intensity, 1.0f);

    // Apply basic lighting
    color *= input.lightingColor;
    color.a = 1.0f;

    PSOutput output;
    output.rt_0_output = color;

    // Object ID for picking
    float4 colorId = float4(0, 0, 0, 1);
    colorId.r = (float)((object_id & 0x00FF0000) >> 16) / 255.0f;
    colorId.g = (float)((object_id & 0x0000FF00) >> 8) / 255.0f;
    colorId.b = (float)((object_id & 0x000000FF)) / 255.0f;
    output.rt_1_output = colorId;

    return output;
}
)";
};
