
#define SET_TEXTURE(TYPE) if(texture_type == TYPE) sampledTexture_Type##TYPE = currentSampledTextureColor;
#define CHECK_TEXTURE_SET(TYPE) all(sampledTexture_Type##TYPE.rgb != 0)

sampler ss : register(s0);
Texture2D shaderTextures[8] : register(t3);

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
    uint4 texture_types[8];
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
    float4 finalColor = (ambientComponent + diffuseComponent + specularComponent);


    // Apply textures
    float4 sampledTexture_Type0 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type10 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type512 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type1024 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type1536 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type1546 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type2050 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type2434 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type4482 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type4490 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type17920 = float4(0, 0, 0, 0);
    float4 sampledTexture_Type17930 = float4(0, 0, 0, 0);

    float2 texCoordsArray[8] =
    {
        input.tex_coords0, input.tex_coords1, input.tex_coords2, input.tex_coords3,
                                 input.tex_coords4, input.tex_coords5, input.tex_coords6, input.tex_coords7
    };
    float a = 0;

    float mult_val = 1;

    for (int j = 0; j < (num_uv_texture_pairs + 3) / 4; ++j)
    {
        for (int k = 0; k < 4; ++k)
        {
            uint uv_set_index = uv_indices[j][k];
            uint texture_index = texture_indices[j][k];
            uint blend_flag = blend_flags[j][k];
            uint texture_type = texture_types[j][k];

            if (j * 4 + k >= num_uv_texture_pairs)
            {
                break;
            }

            for (int t = 0; t < 8; ++t)
            {
                if (t == texture_index)
                {
                    float4 currentSampledTextureColor = shaderTextures[t].Sample(ss, texCoordsArray[uv_set_index]);
                    float alpha = currentSampledTextureColor.a;
                    if (blend_flag == 3 || blend_flag == 6 || blend_flag == 7)
                    {
                        alpha = 1 - alpha;
                    }
                    else if (blend_flag == 0)
                    {
                        alpha = 1;
                    }

					SET_TEXTURE(0);
                    if (CHECK_TEXTURE_SET(0))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type0 * mult_val);
                        mult_val = 2.0;
                    }

					SET_TEXTURE(10);
                    if (CHECK_TEXTURE_SET(10))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type10 * 1);
                    }

					SET_TEXTURE(512);
                    if (CHECK_TEXTURE_SET(512))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type512 * mult_val);
                        mult_val = 2.0;
                    }

					SET_TEXTURE(1024);
                    if (CHECK_TEXTURE_SET(1024))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type1024 * 1);
                    }

					SET_TEXTURE(1536);
                    if (CHECK_TEXTURE_SET(1536))
                    {
                        if (blend_flag == 6)
                            finalColor = saturate(finalColor * sampledTexture_Type1536 * 1);
                        else
                            finalColor = saturate(finalColor * sampledTexture_Type1536 * 2);
                    }

					SET_TEXTURE(1546);
                    if (CHECK_TEXTURE_SET(1546))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type1546 * mult_val);
                        mult_val = 2.0;
                    }

                    SET_TEXTURE(2050);
                    if (CHECK_TEXTURE_SET(2050))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type2050 * mult_val);
                        mult_val = 2.0;
                    }

					SET_TEXTURE(2434);
                    if (CHECK_TEXTURE_SET(2434))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type2434 * mult_val);
                        mult_val = 2.0;
                    }

					SET_TEXTURE(4482);
                    if (CHECK_TEXTURE_SET(4482))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type4482 * mult_val);
                        mult_val = 2.0;
                    }

					SET_TEXTURE(4490);
                    if (CHECK_TEXTURE_SET(4490))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type4490 * mult_val);
                        mult_val = 2.0;
                    }

					SET_TEXTURE(17920);
                    if (CHECK_TEXTURE_SET(17920))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type17920 * 1);
                        mult_val = 2.0;
                    }

					SET_TEXTURE(17930);
                    if (CHECK_TEXTURE_SET(17930))
                    {
                        finalColor = saturate(finalColor * sampledTexture_Type17930 * 1);
                        mult_val = 2.0;
                    }

                        
                    a += alpha * (1.0 - a);
                    break;
                }
            }
        }
    }
    if (a == 0)
    {
        discard;
    }

    finalColor.a = a;

    PSOutput output;
    output.rt_0_output = finalColor;

    float4 colorId = float4(0, 0, 0, 1);
    colorId.r = (float) ((object_id & 0x00FF0000) >> 16) / 255.0f;
    colorId.g = (float) ((object_id & 0x0000FF00) >> 8) / 255.0f;
    colorId.b = (float) ((object_id & 0x000000FF)) / 255.0f;

    // Render target for picking
    output.rt_1_output = colorId;

    return output;
}
