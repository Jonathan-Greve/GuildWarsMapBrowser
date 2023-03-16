#include "RenderConstants.h"

Texture2D shaderTexture : register(t0);
SamplerState SampleType : register(s0);

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


struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoords : TEXCOORD0;
};

float4 main(PixelInputType input) : SV_TARGET
{
    float4 textureColor = shaderTexture.Sample(SampleType, input.texCoords);
    float3 lightDirection = normalize(-directionalLight.direction);
    float3 normal = normalize(input.normal);

    // Compute the diffuse term
    float diffuseIntensity = max(dot(lightDirection, normal), 0.0);

    // Compute the specular term
    float3 viewDirection = normalize(-input.position.xyz);
    float3 reflectDirection = reflect(-lightDirection, normal);
    float specularIntensity = pow(max(dot(viewDirection, reflectDirection), 0.0), 32);

    // Compute the final color
    float4 diffuse = diffuseIntensity * directionalLight.diffuse * textureColor;
    float4 ambient = directionalLight.ambient * textureColor;
    float4 specular = specularIntensity * directionalLight.specular;

    return ambient + diffuse + specular;
}
