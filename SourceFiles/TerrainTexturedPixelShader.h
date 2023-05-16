#pragma once
struct TerrainTexturedPixelShader
{
    static constexpr char shader_ps[] = R"(


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
    float2 texelSize = float2(1.0 / (grid_dim_x - 3), 1.0 / (grid_dim_y - 3));

    // Calculate the tile index
    float2 tileIndex = floor(input.tex_coords0 / texelSize);

    // Compute the corner coordinates based on the tile index
    float2 topLeftTexCoord = tileIndex * texelSize;
    float2 topRightTexCoord = topLeftTexCoord + float2(texelSize.x, 0);
    float2 bottomLeftTexCoord = topLeftTexCoord + float2(0, texelSize.y);
    float2 bottomRightTexCoord = topLeftTexCoord + texelSize;

    // Calculate the texture size
    float2 textureSize = float2(grid_dim_x, grid_dim_y);

    // Convert normalized texture coordinates to integer pixel coordinates
    int2 topLeftCoord = int2(topLeftTexCoord * textureSize);
    int2 topRightCoord = int2(topRightTexCoord * textureSize);
    int2 bottomLeftCoord = int2(bottomLeftTexCoord * textureSize);
    int2 bottomRightCoord = int2(bottomRightTexCoord * textureSize);

    // Load the terrain_texture_indices without interpolation
    int topLeftTexIdx = int(terrain_texture_indices.Load(int3(topLeftCoord, 0)).r * 255.0);
    int topRightTexIdx = int(terrain_texture_indices.Load(int3(topRightCoord, 0)).r * 255.0);
    int bottomLeftTexIdx = int(terrain_texture_indices.Load(int3(bottomLeftCoord, 0)).r * 255.0);
    int bottomRightTexIdx = int(terrain_texture_indices.Load(int3(bottomRightCoord, 0)).r * 255.0);

    // Calculate the UV coordinates for each texture in the textureAtlas
    uint indices[4] = { topLeftTexIdx, topRightTexIdx, bottomLeftTexIdx, bottomRightTexIdx };
    float atlasTileSize = 1.0 / 8.0;
    float innerRegionScale = 0.50; // This is used to extract the 25% of the texture from each corner
    float2 atlasCoords[8];
    int atlasCoordCount = 0;
    float offset_factor_x = 0.12;
    float offset_factor_y = 0.08;
    float scale_factor_x = 1 - offset_factor_x;
    float scale_factor_y = 1 - offset_factor_y;


    float2 cornerOffsets[4] = {
        float2(0.0, 0.0), // TL
        float2(atlasTileSize - atlasTileSize * innerRegionScale, 0.0), // TR
        float2(0.0, atlasTileSize - atlasTileSize * innerRegionScale), // BL
        float2(atlasTileSize - atlasTileSize * innerRegionScale, atlasTileSize - atlasTileSize * innerRegionScale) // BR
    };


    // Case 1: top edge shares vertices
    float2 relativeUV = (input.tex_coords0 - topLeftTexCoord) / texelSize;
    if (topLeftTexIdx == topRightTexIdx) {
        uint index = topLeftTexIdx;
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;

        // offset by offset_factor and make sure to stay within the bounds of the texture
        relativeUV.y = 1.0 - relativeUV.y;
        float2 offset_relativeUV = float2(relativeUV.x * scale_factor_x + offset_factor_x, relativeUV.y * scale_factor_y);

        atlasCoords[atlasCoordCount++] = float2(x, y) + cornerOffsets[0] + offset_relativeUV * (atlasTileSize * innerRegionScale);
    }

    // Case 2: bottom edge shares vertices
    relativeUV = (input.tex_coords0 - topLeftTexCoord) / texelSize;
    if (bottomLeftTexIdx == bottomRightTexIdx) {
        uint index = bottomLeftTexIdx;
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;

        float2 offset_relativeUV = float2(relativeUV.x * scale_factor_x, relativeUV.y * scale_factor_y);

        atlasCoords[atlasCoordCount++] = float2(x, y) + cornerOffsets[0] + offset_relativeUV * (atlasTileSize * innerRegionScale);
    }

    // Case 3: left edge shares vertices
    relativeUV = (input.tex_coords0 - topLeftTexCoord) / texelSize;
    if (topLeftTexIdx == bottomLeftTexIdx) {
        uint index = topLeftTexIdx;
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;

        float2 offset_relativeUV = float2(relativeUV.x * scale_factor_x + offset_factor_x, relativeUV.y * scale_factor_y + offset_factor_y);

        atlasCoords[atlasCoordCount++] = float2(x, y) + cornerOffsets[2] + offset_relativeUV * (atlasTileSize * innerRegionScale);
    }

    // Case 4: right edge shares vertices
    relativeUV = (input.tex_coords0 - topLeftTexCoord) / texelSize;
    if (topRightTexIdx == bottomRightTexIdx) {
        uint index = topRightTexIdx;
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;

        relativeUV.x = 1.0 - relativeUV.x;
        float2 offset_relativeUV = float2(relativeUV.x * scale_factor_x + offset_factor_x, relativeUV.y * scale_factor_y);

        atlasCoords[atlasCoordCount++] = float2(x, y) + cornerOffsets[2] + offset_relativeUV * (atlasTileSize * innerRegionScale);
    }

    // Case 5: top left corner
    relativeUV = (input.tex_coords0 - topLeftTexCoord) / texelSize;
    if (topLeftTexIdx != topRightTexIdx && topLeftTexIdx != bottomLeftTexIdx) {
        uint index = topLeftTexIdx;
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;

        relativeUV.x = 1.0 - relativeUV.x;
        relativeUV.y = 1.0 - relativeUV.y;
        float2 offset_relativeUV = float2(relativeUV.x * scale_factor_x, relativeUV.y * scale_factor_y);

        atlasCoords[atlasCoordCount++] = float2(x, y) + cornerOffsets[3] + offset_relativeUV * (atlasTileSize * innerRegionScale);
    }

    // Case 6: top right corner
    relativeUV = (input.tex_coords0 - topLeftTexCoord) / texelSize;
    if (topRightTexIdx != topLeftTexIdx && topRightTexIdx != bottomRightTexIdx) {
        uint index = topRightTexIdx;
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;

        float2 offset_relativeUV = float2(relativeUV.x * scale_factor_x, relativeUV.y * scale_factor_y + offset_factor_y);

        atlasCoords[atlasCoordCount++] = float2(x, y) + cornerOffsets[1] + offset_relativeUV * (atlasTileSize * innerRegionScale);
    }

    // Case 7: bottom left corner
    relativeUV = (input.tex_coords0 - topLeftTexCoord) / texelSize;
    if (bottomLeftTexIdx != topLeftTexIdx && bottomLeftTexIdx != bottomRightTexIdx) {
        uint index = bottomLeftTexIdx;
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;

        relativeUV.x = 1.0 - relativeUV.x;
        relativeUV.y = 1.0 - relativeUV.y;
        float2 offset_relativeUV = float2(relativeUV.x * scale_factor_x, relativeUV.y * scale_factor_y);

        atlasCoords[atlasCoordCount++] = float2(x, y) + cornerOffsets[1] + offset_relativeUV * (atlasTileSize * innerRegionScale);
    }

    // Case 8: bottom right corner
    relativeUV = (input.tex_coords0 - topLeftTexCoord) / texelSize;
    if (bottomRightTexIdx != topRightTexIdx && bottomRightTexIdx != bottomLeftTexIdx) {
        uint index = bottomRightTexIdx;
        float x = (index % 8) * atlasTileSize;
        float y = (index / 8) * atlasTileSize;

        float2 offset_relativeUV = float2(relativeUV.x * scale_factor_x, relativeUV.y * scale_factor_y);

        atlasCoords[atlasCoordCount++] = float2(x, y) + cornerOffsets[3] + offset_relativeUV * (atlasTileSize * innerRegionScale);
    }

    // Sample the textureAtlas using the calculated UV coordinates
    int non_zero_alphas_count = 0;
    float total_alpha = 0.0;
    float4 sampledColors[8];
    for (int i = 0; i < 8; ++i) {
        if (i == atlasCoordCount) break;
        sampledColors[i] = textureAtlas.Sample(ss, atlasCoords[i]);
        if (sampledColors[i].a > 0) {
            non_zero_alphas_count += 1;
            total_alpha += sampledColors[i].a;
        }
    }

    // Perform texture splatting using the normalized weights
    float4 splattedTextureColor = float4(0.0, 0.0, 0.0, 1.0);
    for (int i = 0; i < 8; ++i) {
        if (i == atlasCoordCount) break;
        if (sampledColors[i].a == 0) continue;
        splattedTextureColor.rgb += sampledColors[i].rgb * sampledColors[i].a / total_alpha;
    }
    // ------------ TEXTURE END ----------------

    float4 outputColor;
    // Multiply the sampled color with the finalColor
    if (input.terrain_height <= water_level) {
        float4 blue_color = float4(0.11, 0.65, 0.81, 1.0); // Water color
        outputColor = finalColor * splattedTextureColor * blue_color;
    }
    else {
        outputColor = finalColor * splattedTextureColor;
    }

    // Return the result
    return outputColor;
}


)";
};
