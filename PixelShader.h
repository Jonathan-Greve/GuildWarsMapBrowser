#pragma once
#include <d3dcompiler.h>

class PixelShader
{
public:
    PixelShader(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
        : m_device(device)
        , m_deviceContext(deviceContext)
    {
    }

    ~PixelShader()
    {
        // Release resources
        if (m_pixelShader)
            m_pixelShader.Reset();
        if (m_samplerState)
            m_samplerState.Reset();
        if (m_shaderResourceView)
            m_shaderResourceView.Reset();
    }

    bool Initialize(const std::wstring& shader_path)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> pixel_shader_blob;
        Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

        HRESULT hr = D3DCompileFromFile(shader_path.c_str(), nullptr, nullptr, "main", "ps_5_0",
                                        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG, 0,
                                        pixel_shader_blob.GetAddressOf(), error_blob.GetAddressOf());

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

    ID3D11ShaderResourceView* GetShaderResourceView() const { return m_shaderResourceView.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
};
