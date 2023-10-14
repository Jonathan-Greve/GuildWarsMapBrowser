#pragma once

struct NewModelPixelShader
{
    static constexpr char shader_ps[] = R"(
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
    float4 lightingColor : COLOR0;
    float2 tex_coords0 : TEXCOORD0;
    float2 tex_coords1 : TEXCOORD1;
    float2 tex_coords2 : TEXCOORD2;
    float2 tex_coords3 : TEXCOORD3;
    float2 tex_coords4 : TEXCOORD4;
    float2 tex_coords5 : TEXCOORD5;
    float2 tex_coords6 : TEXCOORD6;
    float2 tex_coords7 : TEXCOORD7;
    float terrain_height : TEXCOORD8;
    float3x3 TBN : TEXCOORD9;
};

struct PSOutput
{
	float4 rt_0_output : SV_TARGET0; // Goes to first render target (usually the screen)
	float4 rt_1_output : SV_TARGET1; // Goes to second render target
};

PSOutput main(PixelInputType input)
{
    float3 normalFromMap = shaderTextures[1].Sample(ss, input.tex_coords2).rgb * 2.0 - 1.0;

    // Transform the normal from tangent space to world space
    float3 normal = normalize(mul(normalFromMap, input.TBN));

	// Extract the camera position from the view matrix
    float3 cameraPosition = float3(View._41, View._42, View._43);

    // Compute lighting with the Phong reflection model for a directional light
    float3 lightDir = -directionalLight.direction; // The light direction points towards the light source
    float3 viewDir = normalize(cameraPosition - input.position.xyz);
    float3 reflectDir = reflect(-lightDir, normal);

    float shininess = 2;

    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    
    float3 diffuse = diff * directionalLight.ambient.rbg;
    float3 specular = spec * directionalLight.specular.rgb;

    // Use lightingColor for precomputed or ambient lighting
    float3 finalColor = input.lightingColor.rgb + diffuse + specular;

    // Apply textures
	float4 sampledTextureColor = float4(1, 1, 1, 1);
	float2 texCoordsArray[8] =
	{
		input.tex_coords0, input.tex_coords1, input.tex_coords2, input.tex_coords3,
                                 input.tex_coords4, input.tex_coords5, input.tex_coords6, input.tex_coords7
	};

	for (int j = 0; j < (num_uv_texture_pairs + 3) / 4; ++j)
	{
		for (int k = 0; k < 4; ++k)
		{
			uint uv_set_index = uv_indices[j][k];
			uint texture_index = texture_indices[j][k];
			uint blend_flag = blend_flags[j][k];

			if (texture_index == 1)
			{
                continue;
            }

			if (j * 4 + k >= num_uv_texture_pairs)
			{
				break;
			}

			for (int t = 0; t < 8; ++t)
			{
				if (t == texture_index)
				{
					float4 currentSampledTextureColor = shaderTextures[t].Sample(ss, texCoordsArray[uv_set_index]);
					if (blend_flag == 0)
					{
                        currentSampledTextureColor.a = 1;
                    }

                    // Use lerp for blending textures
                    sampledTextureColor = saturate(sampledTextureColor * currentSampledTextureColor);
					break;
				}
			}
		}
	}

    if (sampledTextureColor.a <= 0.0f)
    {
        discard;
    }

    // Multiply the blended color with the finalColor
	if (num_uv_texture_pairs > 0)
	{
		finalColor = finalColor * sampledTextureColor;
	}


	PSOutput output;
    output.rt_0_output = float4(finalColor.rgb, sampledTextureColor.a);

	float4 colorId = float4(0, 0, 0, 1);
	colorId.r = (float) ((object_id & 0x00FF0000) >> 16) / 255.0f;
	colorId.g = (float) ((object_id & 0x0000FF00) >> 8) / 255.0f;
	colorId.b = (float) ((object_id & 0x000000FF)) / 255.0f;

    // Render target for picking
	output.rt_1_output = colorId;

	return output;
}
)";
};
