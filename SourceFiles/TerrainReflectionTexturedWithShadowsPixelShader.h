#pragma once
struct TerrainReflectionTexturedWithShadowsPixelShader
{
    static constexpr char shader_ps[] = R"(
SamplerState ss : register(s0);
SamplerComparisonState shadowSampler : register(s1);
Texture2DArray terrain_texture_array : register(t0);
Texture2D terrain_texture_indices : register(t1);
Texture2D terrain_shadow_map : register(t2);
Texture2D terrain_shadow_map_props : register(t3);

#define MAX_SAMPLES 8
#define ADD_SAMPLE(adjustedUV, texIdx) \
{ \
    float4 s = terrain_texture_array.Sample(ss, float3(adjustedUV.x, adjustedUV.y, texIdx)); \
    samples[sampleCount++] = s; \
    totalAlpha += s.a; \
}

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
    float fog_start_y; // The height at which fog starts.
    float fog_end_y; // The height at which fog ends.
    uint should_render_flags; // Shadows, Water reflection, fog (shadows at bit 0, water reflection at bit 1, fog at bit 2)
};

cbuffer PerObjectCB : register(b1)
{
    matrix World;
    uint4 uv_indices[8];
    uint4 texture_indices[8];
    uint4 blend_flags[8];
    uint4 texture_types[8];
    uint num_uv_texture_pairs;
    uint object_id;
    uint highlight_state; // 0 is not hightlight, 1 is dark green, 2 is lightgreen
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

cbuffer PerTerrainCB : register(b3)
{
    int grid_dim_x;
    int grid_dim_y;
    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float min_z;
    float max_z;
    float water_level;
    float terrain_texture_pad_x;
    float terrain_texture_pad_y;
    float terrain_pad[1];
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 lightingColor : COLOR0;
    float2 tex_coords0 : TEXCOORD0;
    float2 tex_coords1 : TEXCOORD1;
    float2 tex_coords2 : TEXCOORD2;
    float2 tex_coords3 : TEXCOORD3;
    float2 tex_coords4 : TEXCOORD4;
    float2 tex_coords5 : TEXCOORD5;
    float4 reflectionSpacePos : TEXCOORD6;
    float4 lightSpacePos : TEXCOORD7;
    float3 world_position : TEXCOORD8;
    float3x3 TBN : TEXCOORD9;
};

// Assuming quadrants are ordered in a 2x2 grid:
// 0 1
// 2 3

// When top or bottom edges both use the same texture
float2 AdjustUVForQuadrant0(float2 uv, bool isBottomEdge)
{
    float halfU = 0.5;
    float halfV = 0.5;

    float border_u = terrain_texture_pad_x;
    float border_v = terrain_texture_pad_y;

    if (isBottomEdge)
    {
        return lerp(float2(border_u, border_v), float2(halfU - border_u, halfV - border_v), uv);
    }

	// Top edge
    return lerp(float2(border_u, border_v), float2(halfU - border_u, halfV - border_v), float2(1 - uv.x, 1 - uv.y));
}

float2 AdjustUVForQuadrant2(float2 uv, bool isLeftEdge)
{
    float halfU = 0.5;
    float halfV = 0.5;

    float border_u = terrain_texture_pad_x;
    float border_v = terrain_texture_pad_y;

    if (isLeftEdge)
    {
        return lerp(float2(border_u, halfV + border_v), float2(halfU - border_u, 1.0 - border_v), uv);
    }

	// Right edge
    return lerp(float2(border_u, halfV + border_v), float2(halfU - border_u, 1.0 - border_v),
	            float2(1.0 - uv.x, 1 - uv.y));
}

float2 AdjustUVForQuadrant3(float2 uv, bool isBottomRight)
{
    float halfU = 0.5;
    float halfV = 0.5;

    float border_u = terrain_texture_pad_x;
    float border_v = terrain_texture_pad_y;

    if (isBottomRight)
    {
        return lerp(float2(halfU + border_u, halfV + border_v), float2(1.0 - border_u, 1.0 - border_v), uv);
    }

	// Right edge
    return lerp(float2(halfU + border_u, halfV + border_v), float2(1.0 - border_u, 1.0 - border_v),
	            float2(1.0 - uv.x, 1 - uv.y));
}

float2 AdjustUVForQuadrant1(float2 uv, bool isTopRight)
{
    float halfU = 0.5;
    float halfV = 0.5;

    float border_u = terrain_texture_pad_x;
    float border_v = terrain_texture_pad_y;

    if (isTopRight)
    {
        return lerp(float2(halfU + border_u, border_v), float2(1.0 - border_u, halfV - border_v), uv);
    }

	// Right edge
    return lerp(float2(halfU + border_u, border_v), float2(1.0 - border_u, halfV - border_v),
	            float2(1.0 - uv.x, 1 - uv.y));
}

float4 main(PixelInputType input) : SV_TARGET0
{
    if (input.world_position.y <= 0)
    {
        discard;
    }
    
    float4 finalColor = input.lightingColor;

	// ------------ TEXTURE START ----------------
    float2 texelSize = float2(1.0 / grid_dim_x, 1.0 / grid_dim_y);

	// Calculate the tile index
    float2 tileIndex = floor(input.tex_coords0 / texelSize);

	// Compute the corner coordinates based on the tile index
    float2 topLeftTexCoord = tileIndex * texelSize;
    float2 topRightTexCoord = topLeftTexCoord + float2(texelSize.x, 0);
    float2 bottomLeftTexCoord = topLeftTexCoord + float2(0, texelSize.y);
    float2 bottomRightTexCoord = topLeftTexCoord + texelSize;

	// Calculate the texture size
    float2 textureSize = float2(grid_dim_x, grid_dim_y);

	// Convert normalized texture coordinates to integer pixel coordinates
    int2 topLeftCoord = int2(topLeftTexCoord * textureSize);
    int2 topRightCoord = int2(topRightTexCoord * textureSize);
    int2 bottomLeftCoord = int2(bottomLeftTexCoord * textureSize);
    int2 bottomRightCoord = int2(bottomRightTexCoord * textureSize);

	// Load the terrain_texture_indices without interpolation
    int topLeftTexIdx = int(terrain_texture_indices.Load(int3(topLeftCoord, 0)).r * 255.0);
    int topRightTexIdx = int(terrain_texture_indices.Load(int3(topRightCoord, 0)).r * 255.0);
    int bottomLeftTexIdx = int(terrain_texture_indices.Load(int3(bottomLeftCoord, 0)).r * 255.0);
    int bottomRightTexIdx = int(terrain_texture_indices.Load(int3(bottomRightCoord, 0)).r * 255.0);

	// Load texture
    float weight_tl = terrain_shadow_map.Load(int3(topLeftCoord, 0)).r;
    float weight_tr = terrain_shadow_map.Load(int3(topRightCoord, 0)).r;
    float weight_bl = terrain_shadow_map.Load(int3(bottomLeftCoord, 0)).r;
    float weight_br = terrain_shadow_map.Load(int3(bottomRightCoord, 0)).r;

	// Calculate u and v
    float u = frac(input.tex_coords0.x / texelSize.x); // Fractional part of the tile index in x
    float v = frac(input.tex_coords0.y / texelSize.y); // Fractional part of the tile index in y

	// Apply bilinear interpolation
    float blendedWeight = (1 - u) * (1 - v) * weight_tl +
	u * (1 - v) * weight_tr +
	(1 - u) * v * weight_bl +
	u * v * weight_br;


    float4 samples[MAX_SAMPLES];
    int sampleCount = 0;

    float totalAlpha = 0.0;

    if (topLeftTexIdx == bottomLeftTexIdx)
    {
        float2 adjustedUV = AdjustUVForQuadrant2(float2(u, v), true);
		ADD_SAMPLE(adjustedUV, topLeftTexIdx);
    }

    if (topRightTexIdx == bottomRightTexIdx)
    {
        float2 adjustedUV = AdjustUVForQuadrant2(float2(u, v), false);
		ADD_SAMPLE(adjustedUV, topRightTexIdx);
    }

    if (topLeftTexIdx == topRightTexIdx && (topLeftTexIdx != bottomLeftTexIdx || totalAlpha == 0))
    {
        float2 adjustedUV = AdjustUVForQuadrant0(float2(u, v), false);
		ADD_SAMPLE(adjustedUV, topLeftTexIdx);
    }

    if (bottomLeftTexIdx == bottomRightTexIdx && (topRightTexIdx != bottomRightTexIdx || totalAlpha == 0))
    {
        float2 adjustedUV = AdjustUVForQuadrant0(float2(u, v), true);
		ADD_SAMPLE(adjustedUV, bottomLeftTexIdx);
    }

	// TL corner
    if (topLeftTexIdx != topRightTexIdx && topLeftTexIdx != bottomLeftTexIdx)
    {
        float2 adjustedUV = AdjustUVForQuadrant3(float2(u, v), false);
		ADD_SAMPLE(adjustedUV, topLeftTexIdx);
    }

	// BR corner
    if (bottomLeftTexIdx != bottomRightTexIdx && topRightTexIdx != bottomRightTexIdx)
    {
        float2 adjustedUV = AdjustUVForQuadrant3(float2(u, v), true);
		ADD_SAMPLE(adjustedUV, bottomRightTexIdx);
    }

	// TR corner
    if (topLeftTexIdx != topRightTexIdx && topRightTexIdx != bottomRightTexIdx)
    {
        float2 adjustedUV = AdjustUVForQuadrant1(float2(u, v), true);
		ADD_SAMPLE(adjustedUV, topRightTexIdx);
    }

	// BL corner
    if (topLeftTexIdx != bottomLeftTexIdx && bottomLeftTexIdx != bottomRightTexIdx)
    {
        float2 adjustedUV = AdjustUVForQuadrant1(float2(u, v), false);
		ADD_SAMPLE(adjustedUV, bottomLeftTexIdx);
    }

    float4 splattedTextureColor = float4(0.0, 0.0, 0.0, 1.0);
    if (totalAlpha > 0.0)
    {
        for (int i = 0; i < sampleCount; ++i)
        {
            splattedTextureColor.rgb += (samples[i].rgb * samples[i].a) / totalAlpha;
        }
    }
    else
    {
        for (int i = 0; i < sampleCount; ++i)
        {
            splattedTextureColor.rgb += samples[i].rgb / sampleCount;
        }
    }

	// ------------ TEXTURE END ----------------

    float4 outputColor = finalColor * splattedTextureColor;

    float luminance = dot(outputColor.rgb, float3(0.299, 0.587, 0.114));

    float modulatedShadow = max(0.1, lerp(blendedWeight, 1.0, luminance * 0.5));

    outputColor.rgb = outputColor.rgb * modulatedShadow;

    outputColor.a = 1.0f;
    
    bool should_render_shadow = should_render_flags & 1;
    if (should_render_shadow)
    {
        // ============ SHADOW MAP START =====================
        float3 ndcPos = input.lightSpacePos.xyz / input.lightSpacePos.w;

        // Transform position to shadow map texture space
        float2 shadowTexCoord = float2(ndcPos.x * 0.5 + 0.5, -ndcPos.y * 0.5 + 0.5);
        float shadowDepth = input.lightSpacePos.z / input.lightSpacePos.w;

        // Add a bias to reduce shadow acne, especially on steep surfaces
        float bias = max(0.001 * (1.0 - dot(input.normal, -directionalLight.direction)), 0.0005);
        shadowDepth -= bias;

        // PCF
        float shadow = 0.0;
        int pcf_samples = 49;
        float2 shadowmap_texelSize = shadowmap_texel_size;
        for (int x = -3; x <= 3; x++)
        {
            for (int y = -3; y <= 3; y++)
            {
                float2 samplePos = shadowTexCoord + float2(x, y) * shadowmap_texelSize;
                shadow += terrain_shadow_map_props.SampleCmpLevelZero(shadowSampler, samplePos, shadowDepth);
            }
        }

        // Normalize the shadow value
        shadow /= pcf_samples;

        // Apply shadow to final color
        outputColor.rgb *= lerp(0.65, 1.0, shadow);
        // ============ SHADOW MAP END =====================
    }
    
    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float distance = length(cam_position - input.world_position.xyz);

        float fogFactor = (fog_end - distance) / (fog_end - fog_start);
        fogFactor = clamp(fogFactor, 0.20, 1);

        float3 fogColor = fog_color_rgb; // Fog color defined in the constant buffer
        outputColor = lerp(float4(fogColor, 1.0), outputColor, fogFactor);
    }

    return outputColor;
}
)";
};
