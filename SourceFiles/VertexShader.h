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

cbuffer PerFrameCB : register(b0)
{
    DirectionalLight directionalLight;
    float time_elapsed;
    float3 fog_color_rgb;
    float fog_start;
    float fog_end;
    float fog_start_y; // The height at which fog starts.
    float fog_end_y; // The height at which fog ends.
    uint should_render_flags; // Shadows, Water reflection, fog (shadows at bit 0, water reflection at bit 1, fog at bit 2)
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
    matrix directional_light_view;
    matrix directional_light_proj;
    matrix reflection_view;
    matrix reflection_proj;
    float3 cam_position;
    float2 shadowmap_texel_size;
    float2 reflection_texel_size;
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
    float3 tangent : TANGENT;
    float3 bitangent : TANGENT;
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


PixelInputType main(VertexInputType input)
{
    PixelInputType output;

    // Transform the vertex position to clip space
    float4 worldPosition = mul(float4(input.position, 1.0f), World);
    float4 viewPosition = mul(worldPosition, View);
    output.position = mul(viewPosition, Projection);
    output.world_position = worldPosition;

    output.normal = mul(input.normal, (float3x3)World);

    // Pass the texture coordinates to the pixel shader
    output.tex_coords0 = input.tex_coords0;
    output.tex_coords1 = input.tex_coords1;
    output.tex_coords2 = input.tex_coords2;
    output.tex_coords3 = input.tex_coords3;
    output.tex_coords4 = input.tex_coords4;
    output.tex_coords5 = input.tex_coords5;

    // Lighting computation
    if (input.tangent.x == 0.0f && input.tangent.y == 0.0f && input.tangent.z == 0.0f ||
		input.bitangent.x == 0.0f && input.bitangent.y == 0.0f && input.bitangent.z == 0.0f)
    {
        float3 normal = normalize(output.normal);

        // Ensure directionalLight.direction is normalized
        float3 lightDir = normalize(-directionalLight.direction);
        float NdotL = max(dot(normal, lightDir), 0.0);

        float4 ambientComponent = directionalLight.ambient;
        float4 diffuseComponent = directionalLight.diffuse * NdotL;

        // Calculate view direction and ensure normalization
        float3 viewDirection = normalize(cam_position - worldPosition.xyz);

        // Compute half vector and ensure normalization
        float3 halfVector = normalize(lightDir + viewDirection);
        float NdotH = max(dot(normal, halfVector), 0.0);

        float shininess = 80.0; // Shininess factor
        float specularIntensity = pow(NdotH, shininess);
        float4 specularComponent = directionalLight.specular * specularIntensity;

        // Combine lighting components
        output.lightingColor = ambientComponent + diffuseComponent + specularComponent;

    }
    else
    {
		// Calculate the TBN matrix using direct tangent and bitangent
        float3 T = normalize(mul(input.tangent, (float3x3) World)); // Transform tangent
        float3 B = normalize(mul(input.bitangent, (float3x3) World)); // Transform bitangent
        float3 N = normalize(mul(input.normal, (float3x3) World)); // Transform normal

		// Set the TBN matrix
        output.TBN = float3x3(T, B, N);

        output.lightingColor = float4(1, 1, 1, 1);
    }
    
    bool should_render_shadow = should_render_flags & 1;
    bool should_render_water_reflection = should_render_flags & 2;
    
    if (should_render_shadow)
    {
        // Transform position to light space for shadow mapping
        float4 lightViewPosition = mul(worldPosition, directional_light_view);
        output.lightSpacePos = mul(lightViewPosition, directional_light_proj);
    }
    
    if (should_render_water_reflection)
    {
        // Transform position to reflection space for water reflections
        float4 reflectionViewPosition = mul(worldPosition, reflection_view);
        output.reflectionSpacePos = mul(reflectionViewPosition, reflection_proj);
    }

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
