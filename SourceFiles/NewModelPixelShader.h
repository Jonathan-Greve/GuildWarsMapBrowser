#pragma once

struct NewModelPixelShader
{
    static constexpr char shader_ps[] = R"(
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
    float3 cam_position;
    float cam_pad[1];
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
    float3x3 TBN : TEXCOORD9;
};

struct PSOutput
{
	float4 rt_0_output : SV_TARGET0; // Goes to first render target (usually the screen)
	float4 rt_1_output : SV_TARGET1; // Goes to second render target
};

float3 compute_normalmap_lighting(const float3 normalmap_sample, const float3x3 TBN, const float3 pos)
{
    // Transform the normal from tangent space to world space
    const float3 normal = normalize(mul(normalmap_sample, TBN));

    // Compute lighting with the Phong reflection model for a directional light
    const float3 light_dir = -directionalLight.direction; // The light direction points towards the light source
    const float3 view_dir = normalize(cam_position - pos);
    const float3 reflect_dir = reflect(-light_dir, normal);

    const float shininess = 2;

    const float diff = max(dot(normal, light_dir), 0.0);
    const float spec = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);

    const float3 diffuse = diff * directionalLight.ambient.rbg;
    const float3 specular = spec * directionalLight.specular.rgb;

    return float3(1, 1, 1) + diffuse + specular;
};

PSOutput main(PixelInputType input)
{
    float4 sampled_texture_color = float4(1, 1, 1, 1);
    float2 tex_coords_array[8] =
    {
        input.tex_coords0, input.tex_coords1, input.tex_coords2, input.tex_coords3,
                                 input.tex_coords4, input.tex_coords5, input.tex_coords6, input.tex_coords7
    };

    float3 lighting_color = float3(1, 1, 1);

    bool first_non_normalmap_texture_applied = false;
    for (int j = 0; j < (num_uv_texture_pairs + 3) / 4; ++j)
    {
        for (int k = 0; k < 4; ++k)
        {
            uint uv_set_index = uv_indices[j][k];
            const uint texture_index = texture_indices[j][k];
            const uint blend_flag = blend_flags[j][k];
            const uint texture_type = texture_types[j][k] & 0xFF;


            if (j * 4 + k >= num_uv_texture_pairs)
            {
                break;
            }

            for (int t = 0; t < 8; ++t)
            {
                if (t == texture_index)
                {
                    if (texture_type == 2)
                    {
						// Compute normal map lighting
                        const float3 normal_from_map = shaderTextures[t].Sample(ss, tex_coords_array[uv_set_index]).rgb * 2.0 - 1.0;
                        float3 pos = input.position.xyz;
                        lighting_color = compute_normalmap_lighting(normal_from_map, input.TBN, pos);
                    }
                    else
                    {
                        if (!first_non_normalmap_texture_applied)
                        {
                            float4 current_sampled_texture_color = shaderTextures[t].Sample(ss, tex_coords_array[uv_set_index]);
                            if (blend_flag == 0)
                            {
                                current_sampled_texture_color.a = 1;
                            }

                            sampled_texture_color = saturate(sampled_texture_color * current_sampled_texture_color);
                            first_non_normalmap_texture_applied = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (sampled_texture_color.a <= 0.0f)
    {
        discard;
    }

    float3 final_color = lighting_color * sampled_texture_color.rgb;

    PSOutput output;
    output.rt_0_output = float4(final_color, sampled_texture_color.a);

	float4 color_id = float4(0, 0, 0, 1);
	color_id.r = (float) ((object_id & 0x00FF0000) >> 16) / 255.0f;
	color_id.g = (float) ((object_id & 0x0000FF00) >> 8) / 255.0f;
	color_id.b = (float) ((object_id & 0x000000FF)) / 255.0f;

    // Render target for picking
	output.rt_1_output = color_id;

	return output;
}

)";
};
