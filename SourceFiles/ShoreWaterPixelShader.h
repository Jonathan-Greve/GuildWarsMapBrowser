#pragma once

struct ShoreWaterPixelShader
{
    static constexpr char shader_ps[] = R"(
#define CHECK_TEXTURE_SET(TYPE) TYPE == texture_type
#define PI 3.141592653589793
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
    float shore_max_alpha;
    float shore_wave_speed;
    float pad[3];
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

// Function to calculate the wave with an offset c
// Modified Wave function using a sine wave for smoother transitions
float Wave(float x, float c)
{
    x = fmod(x - c, 2 * PI); // Use a full sine wave cycle (2*PI)

    // Scale and shift the sine wave to ensure it stays within the 0 to 1 range
    float wave = 0.5 + 0.5 * sin(x);

    return wave;
}

float WaveDerivative(float x, float c)
{
    x = fmod(x - c, 2 * PI); // Match the cycle used in Wave function

    // Derivative of sine is cosine
    float derivative = cos(x);

    return derivative;
}

// Main Pixel Shader function
float4 main(PixelInputType input) : SV_TARGET
{
    bool use_wave_tex0 = num_uv_texture_pairs > 0 && texture_types[0][0] == 0;
    bool use_wave_tex1 = num_uv_texture_pairs > 1 && texture_types[0][1] == 0;
    
    float4 final_color = float4(1, 1, 1, 1);
    
    float x = time_elapsed * shore_wave_speed * 3;

    // Calculate the wave values with different offsets
    float wave0 = Wave(x, 0.0);
    float wave1 = Wave(x, PI / 2);
    float wave2 = Wave(x, PI);
    float wave3 = Wave(x, 3 * PI / 2);

    // Calculate the UV coordinates
    float2 uv0 = input.tex_coords0;
    float2 uv1 = uv0;
    float2 uv2 = uv0;
    float2 uv3 = uv0;

    // Adjust the Y coordinate of the UVs based on the oscillation
    uv0.y -= wave0 - 0.01;
    uv1.y -= wave1 - 0.01;
    uv2.y -= wave2 - 0.01;
    uv3.y -= wave3 - 0.01;

    // Sample the texture with the adjusted UV coordinates
    float4 sample0 = float4(0, 0, 0, 0);
    float4 sample1 = float4(0, 0, 0, 0);
    float4 sample2 = float4(0, 0, 0, 0);
    float4 sample3 = float4(0, 0, 0, 0);
    
    // Sample 0
    if (use_wave_tex0)
    {
        sample0 = shaderTextures[0].Sample(ss, uv0);
    }
    else if (use_wave_tex1)
    {
        sample0 = shaderTextures[1].Sample(ss, uv0);
    }
    else
    {
        discard;
    }
    
    // Sample 1
    if (use_wave_tex1)
    {
        sample1 = shaderTextures[1].Sample(ss, uv1);
    }
    else if (use_wave_tex0)
    {
        sample1 = shaderTextures[0].Sample(ss, uv1);
    }
    else
    {
        discard;
    }
    
    // Sample 2
    if (use_wave_tex0)
    {
        sample2 = shaderTextures[0].Sample(ss, uv2);
    }
    else if (use_wave_tex1)
    {
        sample2 = shaderTextures[1].Sample(ss, uv2);
    }
    else
    {
        discard;
    }
    
    // Sample 3
    if (use_wave_tex1)
    {
        sample3 = shaderTextures[1].Sample(ss, uv3);
    }
    else if (use_wave_tex0)
    {
        sample3 = shaderTextures[0].Sample(ss, uv3);
    }
    else
    {
        discard;
    }
    
    float waveDerivative0 = WaveDerivative(x, 0.0);
    float waveDerivative1 = WaveDerivative(x, PI / 2);
    float waveDerivative2 = WaveDerivative(x, PI);
    float waveDerivative3 = WaveDerivative(x, 3 * PI / 2);

    // Conditionally adjust the UVs and alphas based on the wave derivatives
    if (waveDerivative0 >= 0)
    {
        sample0.a *= 1 - uv0.y - wave0 * 5;
        sample0.rbg *= 1.5;
    }
    else
    {
        sample0.a *= 1 - uv0.y - wave0 * 1.5;
    }

    if (waveDerivative1 >= 0)
    {
        sample1.a *= 1 - uv1.y - wave1 * 5;
        sample1.rbg *= 1.5;
    }
    else
    {
        sample1.a *= 1 - uv1.y - wave1 * 1.5;
    }

    if (waveDerivative2 >= 0)
    {
        sample2.a *= 1 - uv2.y - wave2 * 5;
        sample2.rbg *= 1.5;
    }
    else
    {
        sample2.a *= 1 - uv2.y - wave2 * 1.5;
    }

    if (waveDerivative3 >= 0)
    {
        sample3.a *= 1 - uv3.y - wave3 * 5;
        sample3.rbg *= 1.5;
    }
    else
    {
        sample3.a *= 1 - uv3.y - wave3 * 1.5;
    }

    
    // Check if the UV coordinates are outside the texture bounds and handle transparency or discard
    if (uv0.y > 1.0 || uv0.y < 0.0 || sample0.a <= 0)
    {
        sample0.a = 0;
    }
    if (uv1.y > 1.0 || uv1.y < 0.0 || sample1.a <= 0)
    {
        sample1.a = 0;
    }
    if (uv2.y > 1.0 || uv2.y < 0.0 || sample2.a <= 0)
    {
        sample2.a = 0;
    }
    
    if (uv3.y > 1.0 || uv3.y < 0.0 || sample3.a <= 0)
    {
        sample3.a = 0;
    }
    
    final_color *= (sample0 * sample0.a + sample1 * sample1.a + sample2 * sample2.a + sample3 * sample3.a) / (sample0.a + sample1.a + sample2.a + sample3.a);
    final_color.a = clamp(final_color.a, 0, shore_max_alpha) * 0.5;
    
    // Fog effect
    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float distance = length(cam_position - input.world_position.xyz);

        float fogFactor = (fog_end - distance) / (fog_end - fog_start);
        fogFactor = clamp(fogFactor, 0.20, 1);

        final_color = lerp(float4(fog_color_rgb, final_color.a), final_color, fogFactor);
    }
    

    return final_color;
}
)";
};
