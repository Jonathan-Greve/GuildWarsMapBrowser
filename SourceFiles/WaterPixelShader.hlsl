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
    float2 tex_scale = float2(-grid_dim_x / 8, -grid_dim_x / 8);
    float scaled_time = time_elapsed / 100;

    // Apply wave offset to texture coordinates
    float2 waterCoord0 = input.tex_coords0.yx * tex_scale - scaled_time;
    float2 normalCoord0 = waterCoord0.yx;

    // Sample water texture at two coordinates
    float4 waterColor0 = shaderTextures[0].Sample(ss, waterCoord0);
    float4 normalColor0 = shaderTextures[1].Sample(ss, normalCoord0);
    final_color = waterColor0;
    
    float normal_r = normalColor0.r; // Convert from [0, 1] range to [-1, 1] 
    float normal_g = normalColor0.g; // Convert from [0, 1] range to [-1, 1] 
    // Reconstruct the Z component. Assume normal points out of the water.
    float normal_y = sqrt(1.0 - normal_r * normal_r - normal_g * normal_g);
    float3 normal = normalize(float3(normal_r, normal_y, normal_g));
    float3 viewDirection = normalize(cam_position - input.world_position.xyz);
    
    float NdotV = dot(normal, viewDirection);
    

    bool should_render_water_reflection = should_render_flags & 2;
    if (should_render_water_reflection)
    {
        // ============ REFLECTION START =====================
        if (input.reflectionSpacePos.y > water_level)
        {
            float3 ndcPos = input.reflectionSpacePos.xyz / input.reflectionSpacePos.w;

            // Transform position to shadow map texture space
            float2 reflectionCoord = float2(ndcPos.x * 0.5 + 0.5, -ndcPos.y * 0.5 + 0.5);
            float4 reflectionColor = shaderTextures[2].Sample(ss, reflectionCoord+normal_g / 40);
    
            // Combine the reflection color with the final color
            // This can be adjusted based on the desired reflection intensity and blending mode
            final_color = lerp(final_color, reflectionColor, 0.5); // Simple blend for demonstration
        }
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
    float4 specularComponent = float4(1, 1, 1, 1) * specularIntensity;

    // Combine lighting components
    final_color *= ambientComponent + diffuseComponent * 0.3 + specularComponent * 0.5;

    // Fog effect
    bool should_render_fog = should_render_flags & 4;
    if (should_render_fog)
    {
        float distance = length(cam_position - input.world_position.xyz);
        float fogFactor = (fog_end - distance) / (fog_end - fog_start);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        final_color = lerp(float4(fog_color_rgb, 1.0), final_color, fogFactor);
    }
    
    float water_alpha = clamp(1 - viewDirection.y, 0.8, 1.0);

    return float4(final_color.rgb, water_alpha);
}