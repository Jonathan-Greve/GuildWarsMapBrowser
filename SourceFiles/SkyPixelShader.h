#pragma once

struct SkyPixelShader
{
    static constexpr char shader_ps[] = R"(
#define CHECK_TEXTURE_SET(TYPE) TYPE == texture_type

sampler ss : register(s0);
SamplerState ssClampLinear : register(s2);
SamplerState ssWrapLinear : register(s3);
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
    float4 final_color = float4(0, 0, 0, 1);
    const float cloud0_strength = 0.55;
    const float cloud1_strength = 0.45;
    
    bool use_sky_background = texture_types[0][0] == 0;
    bool use_clouds_0 = texture_types[0][1] == 0;
    bool use_clouds_1 = texture_types[0][2] == 0;
    bool use_sun = texture_types[0][3] == 0;
    
    if (use_sky_background)
    {
        float4 sampledTextureColor = shaderTextures[0].Sample(ssWrapLinear, input.tex_coords0);
        final_color.rgb = sampledTextureColor.rgb;
    }
    
    if (use_clouds_0)
    {
        float u = (time_elapsed / 1000) + input.tex_coords0.x;
        float v = input.tex_coords0.y;
        
        float4 sampledTextureColor = shaderTextures[1].Sample(ssWrapLinear, float2(u, v));
        final_color.rgb += saturate(sampledTextureColor.rgb) * sampledTextureColor.a * cloud0_strength;
    }
    
    if (use_clouds_1)
    {
        float u = (-time_elapsed / 2000) + input.tex_coords0.x;
        float v = input.tex_coords0.y;
        
        float4 sampledTextureColor = shaderTextures[2].Sample(ssWrapLinear, float2(u, v));
        final_color.rgb += saturate(sampledTextureColor.rgb) * sampledTextureColor.a * cloud1_strength;
    }
    
    if (use_sun)
    {
        float3 view_dir = normalize(input.world_position.xyz - cam_position);
        float3 sun_dir = normalize(-directionalLight.direction);

        float3 basis_up = (abs(sun_dir.y) > 0.98) ? float3(1, 0, 0) : float3(0, 1, 0);
        float3 sun_right = normalize(cross(basis_up, sun_dir));
        float3 sun_up = normalize(cross(sun_dir, sun_right));

        const float sun_disk_radius = 0.03;
        float2 sun_plane = float2(dot(view_dir, sun_right), dot(view_dir, sun_up));
        float2 sun_uv = sun_plane / sun_disk_radius * 0.5 + 0.5;

        float sun_dot = dot(view_dir, sun_dir);
        float sun_mask = saturate((sun_dot - cos(0.03)) / max(cos(0.018) - cos(0.03), 1e-5));

        float4 sampledTextureColor = shaderTextures[3].Sample(ssClampLinear, sun_uv);
        float sun_alpha = sampledTextureColor.a * sun_mask;
        float3 sun_rgb = max(sampledTextureColor.rgb, directionalLight.diffuse.rgb * 1.2);
        final_color.rgb = lerp(final_color.rgb, sun_rgb, sun_alpha);
    }

    final_color.rgb = saturate(final_color.rgb);

    
    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float fog_low = min(fog_start_y, fog_end_y);
        float fog_high = max(fog_start_y, fog_end_y);
        float fog_range = max(fog_high - fog_low, 1.0);
        float fog_band = min(max(fog_range * 0.2, 600.0), 3000.0);
        float fog_base = max(water_level, fog_low);

        float fogFactor = saturate((input.world_position.y - fog_base) / fog_band);
        fogFactor = sqrt(fogFactor);
        final_color.rgb = lerp(fog_color_rgb, final_color.rgb, fogFactor);
    }
    
    return final_color;
}
)";
};
