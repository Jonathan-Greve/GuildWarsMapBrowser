#pragma once
#include <d3dcompiler.h>
#include "Vertex.h"
using Microsoft::WRL::ComPtr;

const char shader_vs[] = R"(
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
    uint num_uv_texture_pairs;
    uint uv_indices[32];
    uint texture_indices[32];
    float pad1[3];
};


cbuffer PerCameraCB : register(b2)
{
    matrix View;
    matrix Projection;
};

struct VertexInputType
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coords0 : TEXCOORD0;
    float2 tex_coords1 : TEXCOORD1;
    float2 tex_coords2 : TEXCOORD2;
    float2 tex_coords3 : TEXCOORD3;
    float2 tex_coords4 : TEXCOORD4;
    float2 tex_coords5 : TEXCOORD5;
    float2 tex_coords6 : TEXCOORD6;
    float2 tex_coords7 : TEXCOORD7;
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


PixelInputType main(VertexInputType input)
{
    PixelInputType output;

    // Transform the vertex position to clip space
    float4 worldPosition = mul(float4(input.position, 1.0f), World);
    float4 viewPosition = mul(worldPosition, View);
    output.position = mul(viewPosition, Projection);

    // Pass the normal and texture coordinates to the pixel shader
    output.normal = mul(input.normal, (float3x3)World);
    output.tex_coords0 = input.tex_coords0;
    output.tex_coords1 = input.tex_coords1;
    output.tex_coords2 = input.tex_coords2;
    output.tex_coords3 = input.tex_coords3;
    output.tex_coords4 = input.tex_coords4;
    output.tex_coords5 = input.tex_coords5;
    output.tex_coords6 = input.tex_coords6;
    output.tex_coords7 = input.tex_coords7;
    output.terrain_height = worldPosition.y;

    return output;
}


)";

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

        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG;
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        // Compile from memory (the string above). Make sure that the shader_vs string is the same as the one in the .hlsl file.
        HRESULT hr = D3DCompile(shader_vs, strlen(shader_vs), NULL, NULL, NULL, "main", "vs_5_0", flags, 0,
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
