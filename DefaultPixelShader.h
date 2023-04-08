#pragma once

struct DefaultPixelShader
{
    static constexpr char shader_ps[] = R"(
sampler ss: register(s0);
Texture2D shaderTextures[8] : register(t0);

struct DirectionalLight
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 direction;
    float pad;
};

cbuffer PerFrameCB: register(b0)
{
    DirectionalLight directionalLight;
};

cbuffer PerObjectCB : register(b1)
{
    matrix World;
    uint4 uv_indices[8];
    uint4 texture_indices[8];
    uint num_uv_texture_pairs;
    float pad1[3];
};

cbuffer PerCameraCB : register(b2)
{
    matrix View;
    matrix Projection;
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
    float2 tex_coords0 : TEXCOORD0;
    float2 tex_coords1 : TEXCOORD1;
    float2 tex_coords2 : TEXCOORD2;
    float2 tex_coords3 : TEXCOORD3;
    float2 tex_coords4 : TEXCOORD4;
    float2 tex_coords5 : TEXCOORD5;
    float2 tex_coords6 : TEXCOORD6;
    float2 tex_coords7 : TEXCOORD7;
    float terrain_height : TEXCOORD8;
};

float4 main(PixelInputType input) : SV_TARGET
{
    // Normalize the input normal
    float3 normal = normalize(input.normal);

    // Calculate the dot product of the normal and light direction
    float NdotL = max(dot(normal, -directionalLight.direction), 0.0);

    // Calculate the ambient and diffuse components
    float4 ambientComponent = directionalLight.ambient;
    float4 diffuseComponent = directionalLight.diffuse * NdotL;

    // Extract the camera position from the view matrix
    float3 cameraPosition = float3(View._41, View._42, View._43);

    // Calculate the specular component using the Blinn-Phong model
    float3 viewDirection = normalize(cameraPosition - input.position.xyz);
    float3 halfVector = normalize(-directionalLight.direction + viewDirection);
    float NdotH = max(dot(normal, halfVector), 0.0);
    float shininess = 80.0; // You can adjust this value for shininess
    float specularIntensity = pow(NdotH, shininess);
    float4 specularComponent = directionalLight.specular * specularIntensity;

    // Combine the ambient, diffuse, and specular components to get the final color
    float4 finalColor = ambientComponent + diffuseComponent + specularComponent;

    // Apply textures
    float4 sampledTextureColor = float4(0, 0, 0, 0);
    for (int j = 0; j < (num_uv_texture_pairs + 3) / 4; ++j)
    {
        for (int k = 0; k < 4; ++k)
        {
            uint uv_set_index = uv_indices[j][k];
            uint texture_index = texture_indices[j][k];

            if (j * 4 + k >= num_uv_texture_pairs)
            {
                break;
            }

            switch (uv_set_index)
            {
            case 0: sampledTextureColor = shaderTextures[0].Sample(ss, input.tex_coords0); break;
            case 1: sampledTextureColor = shaderTextures[1].Sample(ss, input.tex_coords1); break;
            case 2: sampledTextureColor = shaderTextures[2].Sample(ss, input.tex_coords2); break;
            case 3: sampledTextureColor = shaderTextures[3].Sample(ss, input.tex_coords3); break;
            case 4: sampledTextureColor = shaderTextures[4].Sample(ss, input.tex_coords4); break;
            case 5: sampledTextureColor = shaderTextures[5].Sample(ss, input.tex_coords5); break;
            case 6: sampledTextureColor = shaderTextures[6].Sample(ss, input.tex_coords6); break;
            case 7: sampledTextureColor = shaderTextures[7].Sample(ss, input.tex_coords7); break;
            default: break;
            }

            sampledTextureColor += sampledTextureColor * (1.0 / num_uv_texture_pairs); // placeholder
        }
    }

    // Multiply the blended color with the finalColor
    if (num_uv_texture_pairs > 0) {
        float4 finalColor = finalColor * sampledTextureColor;
    }

    // Return the result
    return finalColor;
}


)";
};
