#pragma once

struct NewModelPixelShader
{
    static constexpr char shader_ps[] = R"(
sampler ss : register(s0);
SamplerComparisonState shadowSampler : register(s1);

Texture2D terrain_shadow_map_props : register(t0);
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
    
    bool should_render_model_shadows = should_render_flags & 8;
    if (should_render_model_shadows)
    {
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
        float2 shadowmap_texelSize = shadowmap_texel_size;
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                float2 samplePos = shadowTexCoord + float2(x, y) * shadowmap_texelSize;
                shadow += terrain_shadow_map_props.SampleCmpLevelZero(shadowSampler, samplePos, shadowDepth);
            }
        }

        // Normalize the shadow value
        shadow /= pcf_samples;

        // Apply shadow to final color
        final_color.rgb *= lerp(0.65, 1.0, shadow);
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
        final_color = lerp(fogColor, final_color, fogFactor);
    }
    
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

