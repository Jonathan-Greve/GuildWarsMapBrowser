#pragma once

struct ShoreWaterPixelShader
{
    static constexpr char shader_ps[] = R"(
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
    float water_distortion_tex_scale;
    float water_distortion_scale;
    float water_distortion_tex_speed;
    float water_color_tex_scale;
    float water_color_tex_speed;
    float4 color0;
    float4 color1;
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

// Main Pixel Shader function
float4 main(PixelInputType input) : SV_TARGET
{
    float4 final_color = float4(1, 1, 1, 1);

    float oscillation = 0.5 * (sin(time_elapsed) + 1);

    // Calculate the UV coordinates
    float2 uv = input.tex_coords0;

    // Adjust the Y coordinate of the UVs based on the oscillation
    uv.y = uv.y - oscillation;

    // Sample the texture with the adjusted UV coordinates
    float4 sample0 = shaderTextures[0].Sample(ss, uv);
    sample0.a *= (1 - oscillation);
    // Check if the UV coordinates are outside the texture bounds and handle transparency or discard
    if (uv.y > 1.0 || uv.y < 0.0 || sample0.a <= 0)
    {
        discard;
    }
    else
    {
        // Apply the texture sample to the final color
        final_color *= sample0;
    }

    float3 viewDirection = normalize(cam_position - input.world_position.xyz);
    float3 normal = float3(0, 1, 0);
    
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
    float4 specularComponent = float4(1, 1, 1, 1) * specularIntensity;

    // Combine lighting components
    final_color *= ambientComponent + diffuseComponent * 0.1 + specularComponent * 0.2;
    
    // Fog effect
    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float distance = length(cam_position - input.world_position.xyz);
        float fogFactor = (fog_end - distance) / (fog_end - fog_start);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        final_color = lerp(float4(fog_color_rgb, 1.0), final_color, fogFactor);
    }
    

    return final_color;
}
)";
};
