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
    
    bool use_sky_background = texture_types[0][0] == 0;
    bool use_clouds_0 = texture_types[0][1] == 0;
    bool use_clouds_1 = texture_types[0][2] == 0;
    bool use_sun = texture_types[0][3] == 0;
    
    if (use_sky_background)
    {
        float4 sampledTextureColor = shaderTextures[0].Sample(ss, input.tex_coords0);
        final_color.rgb = sampledTextureColor.rgb;
    }
    
    if (use_clouds_0)
    {
        float u = (time_elapsed / 1000) + input.tex_coords0.x;
        float v = input.tex_coords0.y;
        
        float4 sampledTextureColor = shaderTextures[1].Sample(ss, float2(u, v));
        final_color.rgb = lerp(final_color.rgb, sampledTextureColor.rgb, sampledTextureColor.a);
    }
    
    if (use_clouds_1)
    {
        float u = (-time_elapsed / 2000) + input.tex_coords0.x;
        float v = input.tex_coords0.y;
        
        float4 sampledTextureColor = shaderTextures[2].Sample(ss, float2(u, v));
        final_color.rgb = lerp(final_color.rgb, sampledTextureColor.rgb, sampledTextureColor.a);
    }
    
    //if (use_sun)
    //{
    //    float4 sampledTextureColor = shaderTextures[3].Sample(ss, input.tex_coords0);
    //    final_color.rgb = lerp(sampledTextureColor.rgb, final_color.rgb, sampledTextureColor.a);
    //}
    
    
    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float fogFactor = (input.world_position.y - fog_end_y) / (fog_end_y - fog_start_y);

        fogFactor = clamp(fogFactor, 0, 1);

        float3 fogColor = fog_color_rgb; // Fog color defined in the constant buffer
        final_color = lerp(float4(fogColor, final_color.a), final_color, fogFactor);
    }
    
    return final_color;
}