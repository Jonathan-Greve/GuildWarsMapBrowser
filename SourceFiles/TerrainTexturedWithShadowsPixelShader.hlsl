
sampler ss : register(s0);
Texture2DArray terrain_texture_array : register(t0);
Texture2D terrain_texture_indices : register(t1);
Texture2D terrain_shadow_map : register(t2);

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
    float pad1[2];
};

cbuffer PerCameraCB : register(b2)
{
    matrix View;
    matrix Projection;
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
    float pad[3];
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
    float2 tex_coords6 : TEXCOORD6;
    float2 tex_coords7 : TEXCOORD7;
    float terrain_height : TEXCOORD8;
};

struct PSOutput
{
    float4 rt_0_output : SV_TARGET0; // Goes to first render target (usually the screen)
    float4 rt_1_output : SV_TARGET1; // Goes to second render target
};

// Assuming quadrants are ordered in a 2x2 grid:
// 0 1
// 2 3

// When top or bottom edges both use the same texture
float2 AdjustUVForQuadrant0(float2 uv, bool isBottomEdge)
{
    float halfU = 0.5;
    float halfV = 0.5;

    float border_u = 0.04;
    float border_v = 0.04;

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

    float border_u = 0.04;
    float border_v = 0.04;

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

    float border_u = 0.04;
    float border_v = 0.04;

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

    float border_u = 0.04;
    float border_v = 0.04;

    if (isTopRight)
    {
        return lerp(float2(halfU + border_u, border_v), float2(1.0 - border_u, halfV - border_v), uv);
    }

	// Right edge
    return lerp(float2(halfU + border_u, border_v), float2(1.0 - border_u, halfV - border_v),
	            float2(1.0 - uv.x, 1 - uv.y));
}

PSOutput main(PixelInputType input)
{
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

    float4 outputColor;
    if (input.terrain_height <= water_level)
    {
        float4 blue_color = float4(0.11, 0.65, 0.81, 1.0); // Water color
        outputColor = finalColor * splattedTextureColor * blue_color;
    }
    else
    {
        outputColor = finalColor * splattedTextureColor;
    }

    float luminance = dot(outputColor.rgb, float3(0.299, 0.587, 0.114));

    float modulatedShadow = max(0.1, lerp(blendedWeight, 1.0, luminance * 0.5));

    outputColor.rgb = outputColor.rgb * modulatedShadow;

    outputColor.a = 1.0f;

    PSOutput output;

	// The main render target showing the rendered world
    output.rt_0_output = outputColor;

    float4 colorId = float4(0, 0, 0, 1);
    colorId.r = (float) ((object_id & 0x00FF0000) >> 16) / 255.0f;
    colorId.g = (float) ((object_id & 0x0000FF00) >> 8) / 255.0f;
    colorId.b = (float) ((object_id & 0x000000FF)) / 255.0f;

	// Render target for picking
    output.rt_1_output = colorId;

    return output;
}
