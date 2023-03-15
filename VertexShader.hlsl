cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
};

struct DirectionalLight
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 direction;
    float pad;
};

cbuffer DirectionalLightBuffer : register(b1)
{
    DirectionalLight directionalLight;
};


struct VertexInputType
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoords : TEXCOORD0;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoords : TEXCOORD0;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;

    // Transform the vertex position to clip space
    float4 worldPosition = mul(float4(input.position, 1.0f), World);
    float4 viewPosition = mul(worldPosition, View);
    output.position = mul(viewPosition, Projection);

    // Pass the normal and texture coordinates to the pixel shader
    output.normal = mul(input.normal, (float3x3)World);
    output.texCoords = input.texCoords;

    return output;
}
