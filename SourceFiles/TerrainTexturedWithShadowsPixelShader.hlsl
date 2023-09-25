
sampler ss : register(s0);
Texture2DArray terrain_texture_array : register(t0);
Texture2D terrain_texture_indices : register(t1);
Texture2D terrain_shadow_map : register(t2);

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
};

cbuffer PerObjectCB : register(b1)
{
    matrix World;
    uint4 uv_indices[8];
    uint4 texture_indices[8];
    uint4 blend_flags[8];
    uint num_uv_texture_pairs;
    uint object_id;
    float pad1[2];
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

struct PSOutput
{
    float4 rt_0_output : SV_TARGET0; // Goes to first render target (usually the screen)
    float4 rt_1_output : SV_TARGET1; // Goes to second render target
};

// Function to adjust UV coordinates based on quadrant and border
float2 AdjustUVForQuadrant(float2 uv, int quadrant)
{
    // Assuming quadrants are ordered in a 2x2 grid:
    // 0 1
    // 2 3

    float halfU = 0.5;
    float halfV = 0.5;

    float border_out = 0.04;
    float border_in = 0.04;

    switch (quadrant)
    {
        case 0:
            return lerp(float2(border_out, border_out), float2(halfU - border_in, halfV - border_in), uv);
        case 1:
            return lerp(float2(1.0 - border_out, border_out), float2(halfU + border_in, halfV - border_in), uv);
        case 2:
            return lerp(float2(border_out, 1.0 - border_out), float2(halfU - border_in, halfV + border_in), uv);
        case 3:
            return lerp(float2(1.0 - border_out, 1.0 - border_out), float2(halfU + border_in, halfV + border_in), uv);
    }

    return uv; // default, should not be reached
}

PSOutput main(PixelInputType input)
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

    // Load texture
    float weight_tl = terrain_shadow_map.Load(int3(topLeftCoord, 0)).r;
    float weight_tr = terrain_shadow_map.Load(int3(topRightCoord, 0)).r;
    float weight_bl = terrain_shadow_map.Load(int3(bottomLeftCoord, 0)).r;
    float weight_br = terrain_shadow_map.Load(int3(bottomRightCoord, 0)).r;

    // Calculate u and v
    float u = frac(input.tex_coords0.x / texelSize.x); // Fractional part of the tile index in x
    float v = frac(input.tex_coords0.y / texelSize.y); // Fractional part of the tile index in y

    // Apply bilinear interpolation
    float blendedWeight = (1 - u) * (1 - v) * weight_tl +
                          u * (1 - v) * weight_tr +
                          (1 - u) * v * weight_bl +
                          u * v * weight_br;


    // Perform texture splatting using the normalized weights
    float4 splattedTextureColor = float4(0.0, 0.0, 0.0, 1.0);

    // If all indices are the same, select quadrant pseudo-randomly
    int randomQuadrant = int((bottomLeftTexIdx + int(u * 255.0) + int(v * 255.0)) % 4); // Some pseudo-random logic
    float2 adjustedUV = AdjustUVForQuadrant(float2(u, v), 0);
    splattedTextureColor.rgb = terrain_texture_array.Sample(ss, float4(adjustedUV.x, adjustedUV.y, bottomLeftTexIdx, 0)).rgb;

    // ------------ TEXTURE END ----------------

    float4 outputColor;
    // Multiply the sampled color with the finalColor
    if (input.terrain_height <= water_level)
    {
        float4 blue_color = float4(0.11, 0.65, 0.81, 1.0); // Water color
        outputColor = finalColor * splattedTextureColor * blue_color;
    }
    else
    {
        outputColor = finalColor * splattedTextureColor;
    }

    // Calculate luminance
    float luminance = dot(outputColor.rgb, float3(0.299, 0.587, 0.114));
    // Modulate shadow
    float modulatedShadow = max(0.1, lerp(blendedWeight, 1.0, luminance * 0.5)); // You can adjust the 0.5 factor for different results
    outputColor.rgb = outputColor.rgb * modulatedShadow;

    outputColor.a = 1.0f;

    // Return the result
    PSOutput output;

    // The main render target showing the rendered world
    output.rt_0_output = outputColor;

    float4 colorId = float4(0, 0, 0, 1);
    colorId.r = (float) ((object_id & 0x00FF0000) >> 16) / 255.0f;
    colorId.g = (float) ((object_id & 0x0000FF00) >> 8) / 255.0f;
    colorId.b = (float) ((object_id & 0x000000FF)) / 255.0f;

    // Render target for picking
    output.rt_1_output = colorId;

    return output;
}