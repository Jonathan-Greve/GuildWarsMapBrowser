Texture2D shaderTextures[8] : register(t3);
SamplerState ss : register(s0);

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
    float4 lightSpacePos : TEXCOORD7;
    float3 world_position : TEXCOORD8;
    float3x3 TBN : TEXCOORD9;
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

bool IsTransparent(float alpha, uint blend_flag)
{
    // Modify this function based on your specific transparency and blending logic
    if (blend_flag == 3 || blend_flag == 6 || blend_flag == 7)
    {
        alpha = 1 - alpha;
    }
    else if (blend_flag == 0)
    {
        alpha = 1;
    }

    return alpha == 0;
}

void main(PixelInputType input)
{
    float alpha = 1.0f; // Default opaque
    uint blend_flag = 0;
    
    float2 texCoordsArray[7] =
    {
        input.tex_coords0, input.tex_coords1, input.tex_coords2, input.tex_coords3,
                                 input.tex_coords4, input.tex_coords5, input.tex_coords6
    };

    // Loop through textures
    float a = 0;
    for (int i = 0; i < num_uv_texture_pairs; ++i)
    {
        uint uv_set_index = uv_indices[i / 4][i % 4];
        uint texture_index = texture_indices[i / 4][i % 4];
        uint blend_flag = blend_flags[i / 4][i % 4];

        for (int j = 0; j < 8; ++j)
        {
            if (j == texture_index)
            {
                float4 currentSampledTextureColor = shaderTextures[j].Sample(ss, texCoordsArray[uv_set_index]);
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
            }
        }
    }
    
    if (a <= 0)
    {
        discard;
    }

    // The output depth will be automatically handled by the rasterizer stage;
    // no need to explicitly write to SV_Depth in this scenario.
}