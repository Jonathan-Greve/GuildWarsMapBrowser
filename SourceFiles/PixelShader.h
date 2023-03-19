#pragma once
#include <d3dcompiler.h>

const char shader_ps[] = R"(
sampler ss: register(s0);
Texture2D shaderTexture : register(t0);

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
};

cbuffer PerCameraCB : register(b2)
{
    matrix View;
    matrix Projection;
};


struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoords : TEXCOORD0;
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

    // Sample the texture using the provided texture coordinates and sampler state
    //float4 sampledTextureColor = shaderTexture.Sample(ss, input.texCoords);

    //// Multiply the sampled color with the finalColor
    //float4 outputColor = finalColor * sampledTextureColor;

    // Return the result
    //return outputColor;

    return finalColor;
}
)";

class PixelShader
{
public:
    PixelShader(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
        : m_device(device)
        , m_deviceContext(deviceContext)
    {
    }

    ~PixelShader() { }

    bool Initialize(const std::wstring& shader_path)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG;
#endif
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_blob;
        Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

        HRESULT hr = D3DCompile(shader_ps, strlen(shader_ps), nullptr, nullptr, nullptr, "main", "ps_5_0",
                                flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());

        if (FAILED(hr))
        {
            if (error_blob)
            {
                OutputDebugStringA((char*)error_blob->GetBufferPointer());
            }
            return false;
        }

        hr = m_device->CreatePixelShader(pixel_shader_blob->GetBufferPointer(),
                                         pixel_shader_blob->GetBufferSize(), nullptr,
                                         m_pixelShader.GetAddressOf());

        if (FAILED(hr))
        {
            return false;
        }

        // Create a sampler state
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }

    ID3D11PixelShader* GetShader() const { return m_pixelShader.Get(); }

    ID3D11SamplerState* const* GetSamplerState() const { return m_samplerState.GetAddressOf(); }

private:
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
};
