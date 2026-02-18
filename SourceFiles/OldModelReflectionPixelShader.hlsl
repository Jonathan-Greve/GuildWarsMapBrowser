#define CHECK_TEXTURE_SET(TYPE) TYPE == texture_type

sampler ss : register(s0);
Texture2D shaderTextures[8] : register(t3);

#define DARKGREEN float3(0.4, 1.0, 0.4)
#define LIGHTGREEN float3(0.0, 0.5, 0.0)

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
    uint4 uv_indices[2];
    uint4 texture_indices[2];
    uint4 blend_flags[2];
    uint4 texture_types[2];
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
    float4 reflectionSpacePos : TEXCOORD6;
    float4 lightSpacePos : TEXCOORD7;
    float3 world_position : TEXCOORD8;
    float3x3 TBN : TEXCOORD9;
};

float4 main(PixelInputType input) : SV_TARGET
{
    if (input.world_position.y <= water_level)
    {
        discard;
    }
    
    float4 finalColor = input.lightingColor;

    float2 texCoordsArray[6] =
    {
        input.tex_coords0, input.tex_coords1, input.tex_coords2, input.tex_coords3,
                                 input.tex_coords4, input.tex_coords5
    };
    float a = 0;

    float mult_val = 1;

    uint prev_texture_type = -1;
    uint prev_blend_flag = -1;
    for (int i = 0; i < num_uv_texture_pairs; ++i)
    {
        uint uv_set_index = uv_indices[i / 4][i % 4];
        uint texture_index = texture_indices[i / 4][i % 4];
        uint blend_flag = blend_flags[i / 4][i % 4];
        uint texture_type = texture_types[i / 4][i % 4] & 0xFF;
        uint texture_flag0 = texture_types[i / 4][i % 4] >> 8;

        for (int t = 0; t < 6; ++t)
        {
            if (t == texture_index)
            {
                float4 currentSampledTextureColor = shaderTextures[t].Sample(ss, texCoordsArray[uv_set_index]);
                float alpha = currentSampledTextureColor.a;
                if (blend_flag == 3 || blend_flag == 6 || blend_flag == 7)
                {
                    alpha = 1 - alpha;
                }
                else if (blend_flag == 0)
                {
                    alpha = 1;
                }
                if (blend_flag == 8 && alpha == 0 || (blend_flag == 7 && a == 0))
                {
                    continue;
                }

                a += alpha * (1.0 - a);

                if ((blend_flag == 7 && prev_blend_flag == 8) || blend_flag == 6 || blend_flag == 0)
                {
                    mult_val = 1;
                }
                else
                {
                    mult_val = 2;
                }

                if (blend_flag == 3 || blend_flag == 5)
                {
                    if (prev_texture_type == 1)
                    {
                        finalColor = saturate(currentSampledTextureColor.a * finalColor + currentSampledTextureColor);
                    }
                    else
                    {
                        finalColor = saturate(finalColor.a * currentSampledTextureColor + finalColor);
                    }
                }
                else if (blend_flag == 4 && texture_index > 0)
                {
                    finalColor = saturate(lerp(finalColor, currentSampledTextureColor, currentSampledTextureColor.a));
                }
                else
                {
                    finalColor = saturate(finalColor * currentSampledTextureColor * mult_val);
                }

                prev_texture_type = texture_type;
                prev_blend_flag = blend_flag;
                break;
            }
        }
    }

    if (a <= 0.0f)
    {
        discard;
    }

    finalColor.a = a;
    
    if (highlight_state == 1)
    {
        finalColor.rgb = lerp(finalColor.rgb, DARKGREEN, 0.7);
    }
    else if (highlight_state == 2)
    {
        finalColor.rgb = lerp(finalColor.rgb, LIGHTGREEN, 0.4);
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
        finalColor = lerp(float4(fogColor, finalColor.a), finalColor, fogFactor);
    }
    
    return finalColor;
}