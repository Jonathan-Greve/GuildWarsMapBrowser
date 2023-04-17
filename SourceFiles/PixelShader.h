#pragma once
#include <d3dcompiler.h>
#include "DefaultPixelShader.h"
#include "TerrainCheckeredPixelShader.h"
#include "TerrainDefaultPixelShader.h"
#include "TerrainTexturedPixelShader.h"

enum class PixelShaderType
{
    Default,
    TerrainDefault,
    TerrainCheckered,
    TerrainTextured
};

class PixelShader
{
public:
    PixelShader(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
        : m_device(device)
        , m_deviceContext(deviceContext)
    {
    }

    ~PixelShader() { }

    bool Initialize(PixelShaderType pixel_shader_type)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG;
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_blob;
        Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

        HRESULT hr;
        switch (pixel_shader_type)
        {
        case PixelShaderType::Default:
            hr = D3DCompile(DefaultPixelShader::shader_ps, strlen(DefaultPixelShader::shader_ps), nullptr,
                            nullptr, nullptr, "main", "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(),
                            error_blob.GetAddressOf());
            break;
        case PixelShaderType::TerrainDefault:
            hr = D3DCompile(TerrainDefaultPixelShader::shader_ps,
                            strlen(TerrainDefaultPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                            "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::TerrainCheckered:
            hr = D3DCompile(TerrainCheckeredPixelShader::shader_ps,
                            strlen(TerrainCheckeredPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                            "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::TerrainTextured:
            hr = D3DCompile(TerrainTexturedPixelShader::shader_ps,
                            strlen(TerrainTexturedPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                            "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        default:
            hr = D3DCompile(DefaultPixelShader::shader_ps, strlen(DefaultPixelShader::shader_ps), nullptr,
                            nullptr, nullptr, "main", "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(),
                            error_blob.GetAddressOf());
        }
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
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState(&samplerDesc, m_samplerState.GetAddressOf());
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
