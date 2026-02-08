#pragma once

struct WaterPixelShader
{
    static constexpr char shader_ps[] = R"(
#define CHECK_TEXTURE_SET(TYPE) TYPE == texture_type

sampler ss : register(s0);
Texture2D shaderTextures[8] : register(t0);

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
    float min_z;
    float max_z;
    float water_level;
    float terrain_texture_pad_x;
    float terrain_texture_pad_y;
    float water_distortion_tex_scale;
    float water_distortion_scale;
    float water_distortion_tex_speed;
    float water_color_tex_scale;
    float water_color_tex_speed;
    float water_fresnel;
    float water_specular_scale;
    uint water_technique;
    float water_runtime_param_28;
    float4 color0;
    float4 color1;
    float2 water_flow_dir;
    float2 water_flow_padding;
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

static const float GW_WATER_ANIM_SPEED = 0.01;
static const float GW_WATER_SECONDARY_ANIM_SPEED = 0.02;
static const float GW_WATER_TEX_SCALE = 0.00025;
static const float GW_WATER_SECONDARY_TEX_SCALE = 0.0005;
static const float GW_WATER_ALT_TEX_SCALE = 0.001;
static const float GW_WATER_WAVE_SCALE = 0.04;

float2 NormalizeOrDefault(float2 value, float2 fallback)
{
    float2 result = fallback;
    float len_sq = dot(value, value);
    if (len_sq > 1e-6)
    {
        result = value * rsqrt(len_sq);
    }

    return result;
}

// Main Pixel Shader function
float4 main(PixelInputType input) : SV_TARGET
{
    float4 final_color = float4(0, 0, 0, 1);

    // Matches MapWater_render transform flow in Gw.exe with runtime-packed params:
    // +0x0C waveAmplitude, +0x10 secondaryTexScale, +0x14 waveScale,
    // +0x18 secondarySpeed, +0x1C primaryTexScale.
    bool has_distortion_layer = water_technique >= 1;
    bool has_secondary_animation = water_technique >= 2;
    float2 flow_dir = NormalizeOrDefault(water_flow_dir, normalize(float2(1.0, 1.0)));
    float2 world_uv = input.world_position.xz;

    float primary_scroll = time_elapsed * GW_WATER_ANIM_SPEED * water_color_tex_speed;
    float primary_scale = water_color_tex_scale * GW_WATER_TEX_SCALE;
    // Match MapWater_render matrix + inverse behavior: texture scale applies to
    // world coordinates, while scroll is an additive layer offset.
    float2 color_uv = world_uv * primary_scale + flow_dir * primary_scroll;

    float2 distortion_uv = float2(0.0, 0.0);
    if (has_distortion_layer)
    {
        if (has_secondary_animation)
        {
            float secondary_scroll = time_elapsed * GW_WATER_SECONDARY_ANIM_SPEED * water_distortion_tex_speed;
            float secondary_scale = water_distortion_scale * GW_WATER_SECONDARY_TEX_SCALE;
            distortion_uv = world_uv * secondary_scale - flow_dir * secondary_scroll;
        }
        else
        {
            // Technique 1 uses static perturbation with alternate scale.
            float alt_scale = water_distortion_tex_scale * GW_WATER_ALT_TEX_SCALE;
            distortion_uv = world_uv * alt_scale;
        }
    }

    // Sample water texture at two coordinates
    float4 waterColor0 = shaderTextures[0].Sample(ss, color_uv);
    float4 normalColor0 =
        has_distortion_layer ? shaderTextures[1].Sample(ss, distortion_uv) : float4(0.5, 0.5, 1.0, 1.0);
    final_color = lerp(waterColor0, color0, 0.3);
    
    float3 normal = float3(0.0, 1.0, 0.0);
    float normal_r = 0.0;
    float normal_g = 0.0;
    if (has_distortion_layer)
    {
        normal_r = normalColor0.r * 2 - 1; // Convert from [0, 1] range to [-1, 1] 
        normal_g = normalColor0.g * 2 - 1; // Convert from [0, 1] range to [-1, 1] 
        // Reconstruct the Z component. Assume normal points out of the water.
        float normal_y = sqrt(saturate(1.0 - normal_r * normal_r - normal_g * normal_g));
        normal = normalize(float3(normal_r, normal_y, normal_g));
    }
    float3 viewDirection = normalize(cam_position - input.world_position.xyz);
    
    
    bool should_render_water_reflection = should_render_flags & 2;
    if (should_render_water_reflection)
    {
        // ============ REFLECTION START =====================
        float3 ndcPos = input.reflectionSpacePos.xyz / input.reflectionSpacePos.w;

        // Transform position to shadow map texture space
        float2 reflectionCoord = float2(ndcPos.x * 0.5 + 0.5, -ndcPos.y * 0.5 + 0.5);
        // MapWater_render only applies this projection remap for technique 4.
        if (water_technique == 4)
        {
            float reflection_projection_scale = rcp(max(2.0 * water_runtime_param_28, 0.001));
            reflectionCoord = (reflectionCoord - 0.5) * reflection_projection_scale + 0.5;
        }
        float reflection_perturbation = 0.0;
        if (has_distortion_layer)
        {
            reflection_perturbation =
                has_secondary_animation
                    ? max(water_distortion_scale * GW_WATER_WAVE_SCALE, 0.001)
                    : max(water_distortion_tex_scale * GW_WATER_ALT_TEX_SCALE, 0.001);
        }

        float2 reflection_offset = float2(normal_r, -normal_g) * (reflection_perturbation * 0.5);
        // Reflection render target should not wrap; wrapping causes a visible tiled grid
        // when projection scaling pushes UVs outside [0, 1].
        float2 reflection_uv = saturate(reflectionCoord + reflection_offset);
        float4 reflectionColor = shaderTextures[2].Sample(ss, reflection_uv);
    
        // Combine the reflection color with the final color
        // This can be adjusted based on the desired reflection intensity and blending mode
        final_color = lerp(final_color, lerp(reflectionColor, color1, 0.3), 0.5); // Simple blend for demonstration
        // ============ REFLECTION END =====================
    }
    
    // Ensure directionalLight.direction is normalized
    float3 lightDir = normalize(-directionalLight.direction);
    float NdotL = max(dot(normal, lightDir), 0.0);

    float4 ambientComponent = directionalLight.ambient;
    float4 diffuseComponent = directionalLight.diffuse * NdotL;

    // Compute half vector and ensure normalization
    float3 halfVector = normalize(lightDir + viewDirection);
    float NdotH = max(dot(normal, halfVector), 0.0);

    float shininess = 80.0; // Shininess factor
    float specularIntensity = pow(NdotH, shininess);
    float4 specularComponent = float4(1, 1, 1, 1) * (specularIntensity * water_specular_scale);

    // Combine lighting components
    final_color *= ambientComponent + diffuseComponent * 0.1 + specularComponent * 0.05;

    // Fog effect
    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float distance = length(cam_position - input.world_position.xyz);
        float fogFactor = (fog_end + 10000 - distance) / (fog_end + 10000 - fog_start);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        float3 fog_color = lerp(fog_color_rgb, final_color.rgb, fogFactor);
        final_color = lerp(float4(fog_color, 1.0), final_color, fogFactor);
    }
    
    float technique_alpha_floor = (water_technique == 1) ? 0.82 : 0.7;
    float min_alpha = max(0.5 * (color0.a + color1.a), technique_alpha_floor);
    float water_alpha = clamp(1.2 - viewDirection.y, min_alpha, 1.0);

    return float4(final_color.rgb, water_alpha);
}
)";
};
