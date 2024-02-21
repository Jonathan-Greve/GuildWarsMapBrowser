sampler ss : register(s0);
Texture2D shaderTextures[8] : register(t3);

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

void main(PixelInputType input)
{
    float4 sampled_texture_color = float4(1, 1, 1, 1);
    float2 tex_coords_array[6] =
    {
        input.tex_coords0, input.tex_coords1, input.tex_coords2, input.tex_coords3,
                                 input.tex_coords4, input.tex_coords5
    };

    bool first_non_normalmap_texture_applied = false;
    for (int i = 0; i < num_uv_texture_pairs; ++i)
    {
        uint uv_set_index = uv_indices[i / 4][i % 4];
        uint texture_index = texture_indices[i / 4][i % 4];
        uint blend_flag = blend_flags[i / 4][i % 4];
        uint texture_type = texture_types[i / 4][i % 4] & 0xFF;

        for (int t = 0; t < 6; ++t)
        {
            if (t == texture_index)
            {
                if (texture_type == 2)
                {
                    continue;
                }
                else
                {
                    if (!first_non_normalmap_texture_applied)
                    {
                        sampled_texture_color = shaderTextures[t].Sample(ss, tex_coords_array[uv_set_index]);
                        if (blend_flag == 0)
                        {
                            sampled_texture_color.a = 1;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (sampled_texture_color.a <= 0.35f)
    {
        discard;
    }
}