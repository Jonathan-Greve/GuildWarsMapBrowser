#pragma once
#include <d3dcompiler.h>
#include "Vertex.h"
using Microsoft::WRL::ComPtr;

class VertexShader
{
public:
    VertexShader(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
        : m_device(device)
        , m_deviceContext(deviceContext)
    {
    }

    bool Initialize(const std::wstring& shader_path)
    {
        ComPtr<ID3DBlob> vertex_shader_blob;
        ComPtr<ID3DBlob> error_blob;

        HRESULT hr = D3DCompileFromFile(shader_path.c_str(), nullptr, nullptr, "main", "vs_5_0",
                                        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG, 0,
                                        vertex_shader_blob.GetAddressOf(), error_blob.GetAddressOf());

        if (FAILED(hr))
        {
            if (error_blob)
            {
                OutputDebugStringA((char*)error_blob->GetBufferPointer());
            }
            return false;
        }

        hr = m_device->CreateVertexShader(vertex_shader_blob->GetBufferPointer(),
                                          vertex_shader_blob->GetBufferSize(), nullptr,
                                          m_vertex_shader.GetAddressOf());

        if (FAILED(hr))
        {
            return false;
        }

        // Create the input layout
        hr = m_device->CreateInputLayout(inputLayoutDesc, ARRAYSIZE(inputLayoutDesc),
                                         vertex_shader_blob->GetBufferPointer(),
                                         vertex_shader_blob->GetBufferSize(), m_input_layout.GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }

    ID3D11VertexShader* GetShader() const { return m_vertex_shader.Get(); }
    ID3D11InputLayout* GetInputLayout() const { return m_input_layout.Get(); }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    ComPtr<ID3D11VertexShader> m_vertex_shader;
    ComPtr<ID3D11InputLayout> m_input_layout;
};
