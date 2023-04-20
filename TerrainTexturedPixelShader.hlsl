sampler ss: register(s0);
Texture2D textureAtlas : register(t0);
Texture2D terrain_texture_indices: register(t1);
Texture2D terrain_texture_weights: register(t2);

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
    uint4 blend_flags[8];
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
    float min_z;
    float max_z;
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

    // ------------ TEXTURE START ----------------
    float2 texelSize = float2(1.0 / grid_dim_x, 1.0 / grid_dim_y);

    float2 topLeftTexCoord = input.tex_coords0;
    float2 topRightTexCoord = input.tex_coords0 + float2(0, texelSize.y);
    float2 bottomLeftTexCoord = input.tex_coords0 + float2(texelSize.x, 0);
    float2 bottomRightTexCoord = input.tex_coords0 + texelSize;

    int topLeftTexIdx = int(terrain_texture_indices.Sample(ss, topLeftTexCoord).r * 255);
    int topRightTexIdx = int(terrain_texture_indices.Sample(ss, topRightTexCoord).r * 255);
    int bottomLeftTexIdx = int(terrain_texture_indices.Sample(ss, bottomLeftTexCoord).r * 255);
    int bottomRightTexIdx = int(terrain_texture_indices.Sample(ss, bottomRightTexCoord).r * 255);

    float topLeftAlpha = terrain_texture_weights.Sample(ss, topLeftTexCoord).r;
    float topRightAlpha = terrain_texture_weights.Sample(ss, topRightTexCoord).r;
    float bottomLeftAlpha = terrain_texture_weights.Sample(ss, bottomLeftTexCoord).r;
    float bottomRightAlpha = terrain_texture_weights.Sample(ss, bottomRightTexCoord).r;

    // Calculate the UV coordinates for each texture in the textureAtlas
    uint indices[4] = { topLeftTexIdx, topRightTexIdx, bottomLeftTexIdx, bottomRightTexIdx };
    float atlasTileSize = 1.0 / 8.0;
    float2 atlasCoords[4];
    for (int i = 0; i < 4; ++i) {
        uint index = indices[i];
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;
        float2 tileUpperLeftUV = float2(floor(input.tex_coords0.x / texelSize.x) * texelSize.x, floor(input.tex_coords0.y / texelSize.y) * texelSize.y);
        float2 relativeUV = (input.tex_coords0 - tileUpperLeftUV) / texelSize;
        atlasCoords[i] = float2(x, y) + relativeUV * atlasTileSize;
    }

    // Sample the textureAtlas using the calculated UV coordinates
    float4 sampledColors[4];
    for (int i = 0; i < 4; ++i) {
        sampledColors[i] = textureAtlas.Sample(ss, atlasCoords[i]);
    }

    // Calculate the weights for bilinear interpolation
    float2 weights = frac(input.tex_coords0 / texelSize);
    float4 blendWeights = float4((1 - weights.x) * (1 - weights.y), weights.x * (1 - weights.y), (1 - weights.x) * weights.y, weights.x * weights.y);
    float4 alphas = { topLeftAlpha, topRightAlpha, bottomLeftAlpha, bottomRightAlpha };

    // Blend the sampled colors based on the blend weights
    float4 sampledTextureColor = float4(0, 0, 0, 1);
    for (int j = 0; j < 4; ++j) {
        sampledTextureColor.rgb += sampledColors[j].rgb * blendWeights[j];
    }

    // ------------ TEXTURE END ----------------


    float4 outputColor;
    // Multiply the sampled color with the finalColor
    if (input.terrain_height <= water_level) {
        float4 blue_color = float4(0.11, 0.65, 0.81, 1.0); // Water color
        outputColor = finalColor * sampledTextureColor * blue_color;
    }
    else {
        outputColor = finalColor * sampledTextureColor;
    }

    // Return the result
    return outputColor;
}