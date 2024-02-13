#pragma once
#include <d3dcompiler.h>
#include "OldModelPixelShader.h"
#include "NewModelPixelShader.h"
#include "PickingPixelShader.h"
#include "TerrainCheckeredPixelShader.h"
#include "TerrainDefaultPixelShader.h"
#include "TerrainTexturedPixelShader.h"
#include "TerrainTexturedWithShadowsPixelShader.h"
#include <SkyPixelShader.h>
#include <CloudsPixelShader.h>
#include <OldModelShadowMapPixelShader.h>
#include <WaterPixelShader.h>
#include <TerrainReflectionTexturedWithShadowsPixelShader.h>
#include <OldModelReflectionPixelShader.h>
#include <TerrainShadowMapPixelShader.h>

enum class PixelShaderType
{
    Default,
    TerrainDefault,
    TerrainCheckered,
    TerrainTextured,
    TerrainTexturedWithShadows,
    PickingShader,
    OldModel, // Primarily Prophecies and Factions
    NewModel, // Primarily Nightfall and EotN
    Sky,
    Clouds,
    Water,
    OldModelShadowMap,
    OldModelReflection,
    TerrainReflectionTexturedWithShadows,
    TerrainShadowMap
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
        case PixelShaderType::OldModel:
            hr = D3DCompile(OldModelPixelShader::shader_ps, strlen(OldModelPixelShader::shader_ps), nullptr,
                            nullptr, nullptr, "main", "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(),
                            error_blob.GetAddressOf());
            break;
        case PixelShaderType::NewModel:
            hr = D3DCompile(NewModelPixelShader::shader_ps, strlen(NewModelPixelShader::shader_ps), nullptr,
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
        case PixelShaderType::TerrainTexturedWithShadows:
            hr = D3DCompile(TerrainTexturedWithShadowsPixelShader::shader_ps,
                            strlen(TerrainTexturedWithShadowsPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                            "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::PickingShader:
            hr = D3DCompile(PickingPixelShader::shader_ps,
                            strlen(PickingPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                            "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::Sky:
            hr = D3DCompile(SkyPixelShader::shader_ps,
                strlen(SkyPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::Clouds:
            hr = D3DCompile(CloudsPixelShader::shader_ps,
                strlen(CloudsPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::Water:
            hr = D3DCompile(WaterPixelShader::shader_ps,
                strlen(WaterPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::OldModelShadowMap:
            hr = D3DCompile(OldModelShadowMapPixelShader::shader_ps,
                strlen(OldModelShadowMapPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::OldModelReflection:
            hr = D3DCompile(OldModelReflectionPixelShader::shader_ps,
                strlen(OldModelReflectionPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::TerrainReflectionTexturedWithShadows:
            hr = D3DCompile(TerrainReflectionTexturedWithShadowsPixelShader::shader_ps,
                strlen(TerrainReflectionTexturedWithShadowsPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        case PixelShaderType::TerrainShadowMap:
            hr = D3DCompile(TerrainShadowMapPixelShader::shader_ps,
                strlen(TerrainShadowMapPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
        default:
            hr = D3DCompile(PickingPixelShader::shader_ps,
                            strlen(PickingPixelShader::shader_ps), nullptr, nullptr, nullptr, "main",
                            "ps_5_0", flags, 0, pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());
            break;
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
        D3D11_SAMPLER_DESC shadowSamplerDesc = {};
        shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        shadowSamplerDesc.BorderColor[0] = 1.0f;
        shadowSamplerDesc.BorderColor[1] = 1.0f;
        shadowSamplerDesc.BorderColor[2] = 1.0f;
        shadowSamplerDesc.BorderColor[3] = 1.0f;
        shadowSamplerDesc.MinLOD = 0.f;
        shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        shadowSamplerDesc.MipLODBias = 0.f;
        shadowSamplerDesc.MaxAnisotropy = 0;
        shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        samplerDesc.MipLODBias = 0;

        hr = m_device->CreateSamplerState(&samplerDesc, m_samplerState.GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        hr = m_device->CreateSamplerState(&shadowSamplerDesc, m_samplerStateShadow.GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }

    ID3D11PixelShader* GetShader() const { return m_pixelShader.Get(); }

    ID3D11SamplerState* const* GetSamplerState() const { return m_samplerState.GetAddressOf(); }
    ID3D11SamplerState* const* GetSamplerStateShadow() const { return m_samplerStateShadow.GetAddressOf(); }

private:
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerStateShadow;

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
};
