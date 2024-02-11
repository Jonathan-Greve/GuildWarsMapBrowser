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
    float3 cam_position;
    float2 shadowmap_texel_size;
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
    float4 final_color = float4(0, 0, 0, 1);

    // Water animation based on time
    float2 tex_scale = float2(grid_dim_x, grid_dim_y);
    
    float scaled_time = time_elapsed / 1500;
    
    // Apply wave offset to texture coordinates
    float2 waterCoord0 = input.tex_coords0.yx * tex_scale * float2(scaled_time, sin(scaled_time));
    float2 waterCoord1 = input.tex_coords0.yx * tex_scale * scaled_time;

    // Texture coordinates with animation for normal map
    float2 normalCoord0 = waterCoord0 + 0.01;
    float2 normalCoord1 = waterCoord1 - 0.01;

    // Sample water texture at two coordinates
    float4 waterColor0 = shaderTextures[0].Sample(ss, waterCoord0);
    float4 waterColor1 = shaderTextures[0].Sample(ss, waterCoord1);

    // Sample normal map at two coordinates
    float4 normalColor0 = shaderTextures[1].Sample(ss, normalCoord0);
    float4 normalColor1 = shaderTextures[1].Sample(ss, normalCoord1);

    // Combine the sampled colors
    final_color = 0.5 * (waterColor0 + waterColor1);

    // Combine the normals (this is a simple way; for better results, you might blend them based on some criteria)
    float3 normal0 = normalize(normalColor0.rgb * 2.0 - 1.0); // Convert from [0, 1] to [-1, 1]
    float3 normal1 = normalize(normalColor1.rgb * 2.0 - 1.0);
    float3 normal = normalize(normal0 + normal1);

    // Ensure directionalLight.direction is normalized
    float3 lightDir = normalize(-directionalLight.direction);
    float NdotL = max(dot(normal, lightDir), 0.0);

    float4 ambientComponent = directionalLight.ambient;
    float4 diffuseComponent = directionalLight.diffuse * NdotL;

    // Calculate view direction and ensure normalization
    float3 viewDirection = normalize(cam_position - input.world_position.xyz);

    // Compute half vector and ensure normalization
    float3 halfVector = normalize(lightDir + viewDirection);
    float NdotH = max(dot(normal, halfVector), 0.0);

    float shininess = 180.0; // Shininess factor
    float specularIntensity = pow(NdotH, shininess);
    float4 specularComponent = float4(1, 1, 1, 1) * specularIntensity;

        // Combine lighting components
    final_color *= ambientComponent + diffuseComponent + specularComponent;

    // Fog effect
    float distance = length(cam_position - input.world_position.xyz);
    float fogFactor = (fog_end - distance) / (fog_end - fog_start);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    final_color = lerp(float4(fog_color_rgb, 1.0), final_color, fogFactor);
    
    float water_alpha = clamp(1.0f - viewDirection.y, 0.7, 1.0);

    return float4(final_color.rgb, water_alpha);
}