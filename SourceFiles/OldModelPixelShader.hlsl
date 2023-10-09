#define CHECK_TEXTURE_SET(TYPE) TYPE == texture_type

sampler ss : register(s0);
Texture2D shaderTextures[8] : register(t3);

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

PSOutput main(PixelInputType input)
{
    float4 finalColor = input.lightingColor;

    float2 texCoordsArray[8] =
    {
        input.tex_coords0, input.tex_coords1, input.tex_coords2, input.tex_coords3,
                                 input.tex_coords4, input.tex_coords5, input.tex_coords6, input.tex_coords7
    };
    float a = 0;

    float mult_val = 1;

    for (int j = 0; j < (num_uv_texture_pairs + 3) / 4; ++j)
    {

        uint prev_texture_type = -1;
        uint prev_blend_flag = -1;
        for (int k = 0; k < 4; ++k)
        {
            uint uv_set_index = uv_indices[j][k];
            uint texture_index = texture_indices[j][k];
            uint blend_flag = blend_flags[j][k];
            uint texture_type = texture_types[j][k] & 0xFF;
            uint texture_flag0 = texture_types[j][k] >> 8;

            if (j * 4 + k >= num_uv_texture_pairs)
            {
                break;
            }

            for (int t = 0; t < 8; ++t)
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
    }

    if (a <= 0.0f)
    {
        discard;
    }

    finalColor.a = a;

    PSOutput output;
    output.rt_0_output = finalColor;

    float4 colorId = float4(0, 0, 0, 1);
    colorId.r = (float) ((object_id & 0x00FF0000) >> 16) / 255.0f;
    colorId.g = (float) ((object_id & 0x0000FF00) >> 8) / 255.0f;
    colorId.b = (float) ((object_id & 0x000000FF)) / 255.0f;

    // Render target for picking
    output.rt_1_output = colorId;

    return output;
}