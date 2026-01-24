#pragma once
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "Vertex.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Maximum bones supported for skinning
constexpr size_t MAX_BONES = 256;

// Bone matrices constant buffer structure
struct BoneMatricesCB
{
    XMFLOAT4X4 bones[MAX_BONES];
};

// Skinned vertex shader source embedded as string
const char shader_skinned_vs[] = R"(
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
    float time_elapsed;
    float3 fog_color_rgb;
    float fog_start;
    float fog_end;
    float fog_start_y;
    float fog_end_y;
    uint should_render_flags;
};

cbuffer PerObjectCB : register(b1)
{
    matrix World;
    uint4 uv_indices[2];
    uint4 texture_indices[2];
    uint4 blend_flags[2];
    uint4 texture_types[2];
    uint num_uv_texture_pairs;
    uint object_id;
    uint highlight_state;
    float shore_max_alpha;
    float shore_wave_speed;
    float mesh_alpha;
    float2 pad_object_color;
    float4 object_color;
};

// Generate a unique color for each bone index using HSV to RGB conversion
float3 BoneIndexToColor(uint boneIndex)
{
    float hue = frac((float)boneIndex * 0.618033988749895);
    float saturation = 0.85;
    float value = 0.95;
    float c = value * saturation;
    float h = hue * 6.0;
    float x = c * (1.0 - abs(fmod(h, 2.0) - 1.0));
    float m = value - c;
    float3 rgb;
    if (h < 1.0)      rgb = float3(c, x, 0);
    else if (h < 2.0) rgb = float3(x, c, 0);
    else if (h < 3.0) rgb = float3(0, c, x);
    else if (h < 4.0) rgb = float3(0, x, c);
    else if (h < 5.0) rgb = float3(x, 0, c);
    else              rgb = float3(c, 0, x);
    return rgb + m;
}

cbuffer PerCameraCB : register(b2)
{
    matrix View;
    matrix Projection;
    matrix directional_light_view;
    matrix directional_light_proj;
    matrix reflection_view;
    matrix reflection_proj;
    float3 cam_position;
    float2 shadowmap_texel_size;
    float2 reflection_texel_size;
};

cbuffer BoneMatricesCB : register(b3)
{
    matrix bones[256];
};

struct SkinnedVertexInputType
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
    float3 tangent : TANGENT0;
    float3 bitangent : TANGENT1;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
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
    float4 reflectionSpacePos : TEXCOORD6;
    float4 lightSpacePos : TEXCOORD7;
    float3 world_position : TEXCOORD8;
    float3x3 TBN : TEXCOORD9;
};

float4 SkinPosition(float3 pos, uint4 indices, float4 weights)
{
    float4 result = float4(0, 0, 0, 0);
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        if (weights[i] > 0.0f)
        {
            result += weights[i] * mul(float4(pos, 1.0f), bones[indices[i]]);
        }
    }
    if (result.w == 0.0f)
    {
        return float4(pos, 1.0f);
    }
    return result;
}

float3 SkinNormal(float3 normal, uint4 indices, float4 weights)
{
    float3 result = float3(0, 0, 0);
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        if (weights[i] > 0.0f)
        {
            result += weights[i] * mul(normal, (float3x3)bones[indices[i]]);
        }
    }
    float len = length(result);
    if (len > 0.001f)
    {
        return result / len;
    }
    return normal;
}

PixelInputType main(SkinnedVertexInputType input)
{
    PixelInputType output;

    float4 skinnedPosition = SkinPosition(input.position, input.boneIndices, input.boneWeights);
    float3 skinnedNormal = SkinNormal(input.normal, input.boneIndices, input.boneWeights);

    float4 worldPosition = mul(skinnedPosition, World);
    float4 viewPosition = mul(worldPosition, View);
    output.position = mul(viewPosition, Projection);
    output.world_position = worldPosition.xyz;

    output.normal = mul(skinnedNormal, (float3x3)World);

    output.tex_coords0 = input.tex_coords0;
    output.tex_coords1 = input.tex_coords1;
    output.tex_coords2 = input.tex_coords2;
    output.tex_coords3 = input.tex_coords3;
    output.tex_coords4 = input.tex_coords4;
    output.tex_coords5 = input.tex_coords5;

    // Color by bone index mode
    // highlight_state == 3: remapped skeleton bone (boneIndices[0])
    // highlight_state == 4: raw FA0 palette index (boneIndices[1])
    if (highlight_state >= 3)
    {
        uint boneIdx = (highlight_state == 4) ? input.boneIndices[1] : input.boneIndices[0];
        float3 boneColor = BoneIndexToColor(boneIdx);
        output.lightingColor = float4(boneColor, 1.0);
        output.TBN = float3x3(float3(1,0,0), float3(0,1,0), float3(0,0,1));
    }
    else if (input.tangent.x == 0.0f && input.tangent.y == 0.0f && input.tangent.z == 0.0f ||
        input.bitangent.x == 0.0f && input.bitangent.y == 0.0f && input.bitangent.z == 0.0f)
    {
        float3 normal = normalize(output.normal);
        float3 lightDir = normalize(-directionalLight.direction);
        float NdotL = max(dot(normal, lightDir), 0.0);

        float4 ambientComponent = directionalLight.ambient;
        float4 diffuseComponent = directionalLight.diffuse * NdotL;

        float3 viewDirection = normalize(cam_position - worldPosition.xyz);
        float3 halfVector = normalize(lightDir + viewDirection);
        float NdotH = max(dot(normal, halfVector), 0.0);

        float shininess = 80.0;
        float specularIntensity = pow(NdotH, shininess);
        float4 specularComponent = directionalLight.specular * specularIntensity;

        output.lightingColor = ambientComponent + diffuseComponent + specularComponent;
    }
    else
    {
        float3 skinnedTangent = SkinNormal(input.tangent, input.boneIndices, input.boneWeights);
        float3 skinnedBitangent = SkinNormal(input.bitangent, input.boneIndices, input.boneWeights);

        float3 T = normalize(mul(skinnedTangent, (float3x3)World));
        float3 B = normalize(mul(skinnedBitangent, (float3x3)World));
        float3 N = normalize(mul(skinnedNormal, (float3x3)World));

        output.TBN = float3x3(T, B, N);
        output.lightingColor = float4(1, 1, 1, 1);
    }

    bool should_render_shadow = should_render_flags & 1;
    bool should_render_water_reflection = should_render_flags & 2;

    if (should_render_shadow)
    {
        float4 lightViewPosition = mul(worldPosition, directional_light_view);
        output.lightSpacePos = mul(lightViewPosition, directional_light_proj);
    }

    if (should_render_water_reflection)
    {
        float4 reflectionViewPosition = mul(worldPosition, reflection_view);
        output.reflectionSpacePos = mul(reflectionViewPosition, reflection_proj);
    }

    return output;
}
)";

class SkinnedVertexShader
{
public:
    SkinnedVertexShader(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
        : m_device(device)
        , m_deviceContext(deviceContext)
    {
    }

    bool Initialize()
    {
        ComPtr<ID3DBlob> vertex_shader_blob;
        ComPtr<ID3DBlob> error_blob;

        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG;
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT hr = D3DCompile(shader_skinned_vs, strlen(shader_skinned_vs), NULL, NULL, NULL, "main", "vs_5_0", flags, 0,
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

        // Input layout for skinned vertices (with bone indices and weights)
        D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 5, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 6, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        // Create input layout for skinned vertices
        hr = m_device->CreateInputLayout(inputLayoutDesc, ARRAYSIZE(inputLayoutDesc),
                                         vertex_shader_blob->GetBufferPointer(),
                                         vertex_shader_blob->GetBufferSize(), m_input_layout.GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        // Create bone matrices constant buffer
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.ByteWidth = sizeof(BoneMatricesCB);
        cbDesc.StructureByteStride = sizeof(BoneMatricesCB);

        hr = m_device->CreateBuffer(&cbDesc, nullptr, m_boneMatricesCB.GetAddressOf());
        if (FAILED(hr))
        {
            return false;
        }

        // Initialize bone matrices to identity
        BoneMatricesCB identityBones;
        for (size_t i = 0; i < MAX_BONES; i++)
        {
            XMStoreFloat4x4(&identityBones.bones[i], XMMatrixIdentity());
        }
        UpdateBoneMatrices(identityBones.bones, MAX_BONES);

        return true;
    }

    void UpdateBoneMatrices(const XMFLOAT4X4* matrices, size_t count)
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = m_deviceContext->Map(m_boneMatricesCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            BoneMatricesCB* data = static_cast<BoneMatricesCB*>(mappedResource.pData);
            size_t copyCount = std::min(count, MAX_BONES);
            memcpy(data->bones, matrices, copyCount * sizeof(XMFLOAT4X4));

            // Fill remaining with identity
            for (size_t i = copyCount; i < MAX_BONES; i++)
            {
                XMStoreFloat4x4(&data->bones[i], XMMatrixIdentity());
            }

            m_deviceContext->Unmap(m_boneMatricesCB.Get(), 0);
        }
    }

    void Bind()
    {
        m_deviceContext->VSSetShader(m_vertex_shader.Get(), nullptr, 0);
        m_deviceContext->IASetInputLayout(m_input_layout.Get());

        // Bind bone matrices to slot 3
        ID3D11Buffer* constantBuffers[] = { m_boneMatricesCB.Get() };
        m_deviceContext->VSSetConstantBuffers(3, 1, constantBuffers);
    }

    ID3D11VertexShader* GetShader() const { return m_vertex_shader.Get(); }
    ID3D11InputLayout* GetInputLayout() const { return m_input_layout.Get(); }
    ID3D11Buffer* GetBoneMatricesBuffer() const { return m_boneMatricesCB.Get(); }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    ComPtr<ID3D11VertexShader> m_vertex_shader;
    ComPtr<ID3D11InputLayout> m_input_layout;
    ComPtr<ID3D11Buffer> m_boneMatricesCB;
};
