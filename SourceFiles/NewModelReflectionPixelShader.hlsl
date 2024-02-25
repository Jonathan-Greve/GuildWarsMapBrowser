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

float4 main(PixelInputType input) : SV_TARGET
{
    if (input.world_position.y <= water_level)
    {
        discard;
    }
    
    float4 sampled_texture_color = float4(1, 1, 1, 1);
    float2 tex_coords_array[6] =
    {
        input.tex_coords0, input.tex_coords1, input.tex_coords2, input.tex_coords3,
                                 input.tex_coords4, input.tex_coords5
    };

    float3 lighting_color = float3(1, 1, 1);

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

    if (sampled_texture_color.a <= 0.0f)
    {
        discard;
    }

    float3 final_color = lighting_color * sampled_texture_color.rgb;
    
    if (highlight_state == 1)
    {
        final_color.rgb = lerp(final_color.rgb, DARKGREEN, 0.7);
    }
    else if (highlight_state == 2)
    {
        final_color.rgb = lerp(final_color.rgb, LIGHTGREEN, 0.4);
    }

    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float distance = length(cam_position - input.world_position.xyz);

        float fogFactor = (fog_end - distance) / (fog_end - fog_start);
        fogFactor = clamp(fogFactor, 0.20, 1);

        float3 fogColor = fog_color_rgb; // Fog color defined in the constant buffer
        final_color = lerp(fogColor, final_color, fogFactor);
    }
    
    return float4(final_color, sampled_texture_color.a);
}