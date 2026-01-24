// ============================================================================
// Skinned Vertex Shader for Animated Models
// Extends the standard vertex shader with linear blend skinning support
// ============================================================================

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
    float fog_start_y;
    float fog_end_y;
    uint should_render_flags;
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
    uint highlight_state; // 0=normal, 1=dark green, 2=light green, 3=color by bone index
    float shore_max_alpha;
    float shore_wave_speed;
    float mesh_alpha;
    float2 pad_object_color;
    float4 object_color;
};

// Generate a unique color for each bone index using HSV to RGB conversion
float3 BoneIndexToColor(uint boneIndex)
{
    // Use golden ratio to spread colors across the hue spectrum
    float hue = frac((float)boneIndex * 0.618033988749895);
    float saturation = 0.85;
    float value = 0.95;

    float c = value * saturation;
    float h = hue * 6.0;
    float x = c * (1.0 - abs(fmod(h, 2.0) - 1.0));
    float m = value - c;

    float3 rgb;
    if (h < 1.0)      rgb = float3(c, x, 0);
    else if (h < 2.0) rgb = float3(x, c, 0);
    else if (h < 3.0) rgb = float3(0, c, x);
    else if (h < 4.0) rgb = float3(0, x, c);
    else if (h < 5.0) rgb = float3(x, 0, c);
    else              rgb = float3(c, 0, x);

    return rgb + m;
}

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

// Bone matrix buffer for skinning (up to 256 bones)
cbuffer BoneMatricesCB : register(b3)
{
    matrix bones[256];
};

// Skinned vertex input - includes bone indices and weights
struct SkinnedVertexInputType
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coords0 : TEXCOORD0;
    float2 tex_coords1 : TEXCOORD1;
    float2 tex_coords2 : TEXCOORD2;
    float2 tex_coords3 : TEXCOORD3;
    float2 tex_coords4 : TEXCOORD4;
    float2 tex_coords5 : TEXCOORD5;
    float2 tex_coords6 : TEXCOORD6;
    float2 tex_coords7 : TEXCOORD7;
    float3 tangent : TANGENT0;
    float3 bitangent : TANGENT1;
    uint4 boneIndices : BLENDINDICES;   // Up to 4 bone influences
    float4 boneWeights : BLENDWEIGHT;   // Corresponding weights
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

// Applies linear blend skinning to a position
// Uses up to 4 bone influences per vertex
float4 SkinPosition(float3 pos, uint4 indices, float4 weights)
{
    float4 result = float4(0, 0, 0, 0);

    // Sum contributions from all influencing bones
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        if (weights[i] > 0.0f)
        {
            result += weights[i] * mul(float4(pos, 1.0f), bones[indices[i]]);
        }
    }

    // If no weights were applied (shouldn't happen), return original position
    if (result.w == 0.0f)
    {
        return float4(pos, 1.0f);
    }

    return result;
}

// Applies linear blend skinning to a normal/vector
// Similar to position but without translation (w=0)
float3 SkinNormal(float3 normal, uint4 indices, float4 weights)
{
    float3 result = float3(0, 0, 0);

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        if (weights[i] > 0.0f)
        {
            // Use 3x3 part of bone matrix for normals
            result += weights[i] * mul(normal, (float3x3)bones[indices[i]]);
        }
    }

    // Normalize the result to handle any scaling in the bone matrices
    float len = length(result);
    if (len > 0.001f)
    {
        return result / len;
    }

    return normal;
}

PixelInputType main(SkinnedVertexInputType input)
{
    PixelInputType output;

    // Apply skinning to position
    float4 skinnedPosition = SkinPosition(input.position, input.boneIndices, input.boneWeights);

    // Apply skinning to normal
    float3 skinnedNormal = SkinNormal(input.normal, input.boneIndices, input.boneWeights);

    // Transform the skinned vertex position to clip space
    float4 worldPosition = mul(skinnedPosition, World);
    float4 viewPosition = mul(worldPosition, View);
    output.position = mul(viewPosition, Projection);
    output.world_position = worldPosition.xyz;

    // Transform skinned normal to world space
    output.normal = mul(skinnedNormal, (float3x3)World);

    // Pass the texture coordinates to the pixel shader
    output.tex_coords0 = input.tex_coords0;
    output.tex_coords1 = input.tex_coords1;
    output.tex_coords2 = input.tex_coords2;
    output.tex_coords3 = input.tex_coords3;
    output.tex_coords4 = input.tex_coords4;
    output.tex_coords5 = input.tex_coords5;

    // Color by bone index mode
    // highlight_state == 3: remapped skeleton bone (boneIndices[0])
    // highlight_state == 4: raw FA0 palette index (boneIndices[1])
    if (highlight_state >= 3)
    {
        // Choose which bone index to use based on mode
        uint boneIdx = (highlight_state == 4) ? input.boneIndices[1] : input.boneIndices[0];
        float3 boneColor = BoneIndexToColor(boneIdx);
        output.lightingColor = float4(boneColor, 1.0);
        output.TBN = float3x3(float3(1,0,0), float3(0,1,0), float3(0,0,1));
    }
    // Lighting computation
    else if (input.tangent.x == 0.0f && input.tangent.y == 0.0f && input.tangent.z == 0.0f ||
        input.bitangent.x == 0.0f && input.bitangent.y == 0.0f && input.bitangent.z == 0.0f)
    {
        float3 normal = normalize(output.normal);

        // Ensure directionalLight.direction is normalized
        float3 lightDir = normalize(-directionalLight.direction);
        float NdotL = max(dot(normal, lightDir), 0.0);

        float4 ambientComponent = directionalLight.ambient;
        float4 diffuseComponent = directionalLight.diffuse * NdotL;

        // Calculate view direction and ensure normalization
        float3 viewDirection = normalize(cam_position - worldPosition.xyz);

        // Compute half vector and ensure normalization
        float3 halfVector = normalize(lightDir + viewDirection);
        float NdotH = max(dot(normal, halfVector), 0.0);

        float shininess = 80.0;
        float specularIntensity = pow(NdotH, shininess);
        float4 specularComponent = directionalLight.specular * specularIntensity;

        // Combine lighting components
        output.lightingColor = ambientComponent + diffuseComponent + specularComponent;
    }
    else
    {
        // Apply skinning to tangent and bitangent
        float3 skinnedTangent = SkinNormal(input.tangent, input.boneIndices, input.boneWeights);
        float3 skinnedBitangent = SkinNormal(input.bitangent, input.boneIndices, input.boneWeights);

        // Calculate the TBN matrix using skinned tangent and bitangent
        float3 T = normalize(mul(skinnedTangent, (float3x3)World));
        float3 B = normalize(mul(skinnedBitangent, (float3x3)World));
        float3 N = normalize(mul(skinnedNormal, (float3x3)World));

        // Set the TBN matrix
        output.TBN = float3x3(T, B, N);

        output.lightingColor = float4(1, 1, 1, 1);
    }

    bool should_render_shadow = should_render_flags & 1;
    bool should_render_water_reflection = should_render_flags & 2;

    if (should_render_shadow)
    {
        // Transform position to light space for shadow mapping
        float4 lightViewPosition = mul(worldPosition, directional_light_view);
        output.lightSpacePos = mul(lightViewPosition, directional_light_proj);
    }

    if (should_render_water_reflection)
    {
        // Transform position to reflection space for water reflections
        float4 reflectionViewPosition = mul(worldPosition, reflection_view);
        output.reflectionSpacePos = mul(reflectionViewPosition, reflection_proj);
    }

    return output;
}
