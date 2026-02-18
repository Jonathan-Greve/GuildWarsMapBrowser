Texture2DArray atlas : register(t0);
Texture2D terrain_shadow_map_props : register(t3);
SamplerState samLinear : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct DirectionalLight
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 direction;
    float pad;
};

cbuffer PerFrameCB : register(b0)
{
    DirectionalLight directionalLight;
    float time_elapsed;
    float3 fog_color_rgb;
    float fog_start;
    float fog_end;
    float fog_start_y;
    float fog_end_y;
    uint should_render_flags;
};

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

cbuffer PerCameraCB : register(b2)
{
    matrix View;
    matrix Projection;
    matrix directional_light_view;
    matrix directional_light_proj;
    matrix reflection_view;
    matrix reflection_proj;
    float3 cam_position;
    float2 shadowmap_texel_size;
    float2 reflection_texel_size;
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

float4 SampleAtlas(float2 uv, float2 dx, float2 dy)
{
    // Decode Atlas UV to Layer + Local UV
    // Atlas Size: 2048, Tile Size: 256
    // Grid: 8x8
    
    float2 pixelPos = uv * 2048.0f;
    uint2 slotCoords = uint2(pixelPos) / 256;
    uint slotIndex = slotCoords.y * 8 + slotCoords.x;
    
    // Neutral texture is Slot 0 (Layer -1, so effectively transparent)
    if (slotIndex == 0)
    {
        return float4(0, 0, 0, 0);
    }
    
    uint layerIndex = slotIndex - 1;
    
    // Local UV within the 256x256 tile
    // Using frac ensures we handle the border offset correctly relative to the tile
    float2 localUV = frac(pixelPos / 256.0f);
    
    return atlas.SampleGrad(samLinear, float3(localUV, layerIndex), dx, dy);
}

PSOutput main(PixelInputType input)
{
    // Calculate derivatives for mipmapping (scaled for 256x256 tiles vs 2048x2048 atlas)
    // We calculate per-UV derivatives to handle rotation/scaling correctly
    float2 dx0 = ddx(input.uv0) * 8.0f;
    float2 dy0 = ddy(input.uv0) * 8.0f;
    float2 dx1 = ddx(input.uv1) * 8.0f;
    float2 dy1 = ddy(input.uv1) * 8.0f;
    float2 dx2 = ddx(input.uv2) * 8.0f;
    float2 dy2 = ddy(input.uv2) * 8.0f;

    // Sample all texture layers
    float4 t0 = SampleAtlas(input.uv0, dx0, dy0);
    float4 t1 = SampleAtlas(input.uv1, dx1, dy1);
    float4 t2 = SampleAtlas(input.uv2, dx2, dy2);

    // Progressive alpha blending
    float4 result = t0;
    result = lerp(result, t1, t1.a);
    result = lerp(result, t2, t2.a);
    
    // Apply lighting
    float4 color = result * 1.4 * input.lightingColor;
    color.a = 1.0f;

    bool should_render_shadow = should_render_flags & 1;
    if (should_render_shadow)
    {
        // ============ SHADOW MAP START =====================
        float3 ndcPos = input.lightSpacePos.xyz / input.lightSpacePos.w;

        // Transform position to shadow map texture space
        float2 shadowTexCoord = float2(ndcPos.x * 0.5 + 0.5, -ndcPos.y * 0.5 + 0.5);
        float shadowDepth = input.lightSpacePos.z / input.lightSpacePos.w;

        // Add a bias to reduce shadow acne, especially on steep surfaces
        float bias = max(0.001 * (1.0 - dot(normalize(input.normal), -normalize(directionalLight.direction))), 0.0005);
        shadowDepth += bias;

        // PCF
        float shadow = 0.0;
        int pcf_samples = 9;
        float2 texelSize = shadowmap_texel_size;
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                float2 samplePos = shadowTexCoord + float2(x, y) * texelSize;
                shadow += terrain_shadow_map_props.SampleCmpLevelZero(shadowSampler, samplePos, shadowDepth);
            }
        }

        // Normalize the shadow value
        shadow /= pcf_samples;

        // Apply shadow to final color
        color.rgb *= lerp(0.65, 1.0, shadow);
        // ============ SHADOW MAP END =====================
    }

    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float distance = length(cam_position - input.world_position.xyz);

        float fogFactorDistance = 1.0;
        float fogDistanceDenom = fog_end - fog_start;
        if (abs(fogDistanceDenom) >= 1e-3)
        {
            fogFactorDistance = saturate((fog_end - distance) / fogDistanceDenom);
        }

        float fogFactorHeight = 1.0;
        float fogHeightDenom = fog_end_y - fog_start_y;
        if (abs(fogHeightDenom) >= 1e-3)
        {
            fogFactorHeight = saturate((fog_end_y - input.world_position.y) / fogHeightDenom);
        }

        float fogFactor = min(fogFactorDistance, fogFactorHeight);
        fogFactor = clamp(fogFactor, 0.20, 1);

        float3 fogColor = fog_color_rgb; // Fog color defined in the constant buffer
        color = lerp(float4(fogColor, 1.0), color, fogFactor);
    }

    PSOutput output;
    output.rt_0_output = color;

    float4 colorId = float4(0, 0, 0, 1);
    colorId.r = (float) ((object_id & 0x00FF0000) >> 16) / 255.0f;
    colorId.g = (float) ((object_id & 0x0000FF00) >> 8) / 255.0f;
    colorId.b = (float) ((object_id & 0x000000FF)) / 255.0f;
    output.rt_1_output = colorId;

    return output;
}
