#pragma once
#include "InputManager.h"
#include "Camera.h"
#include "MeshManager.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "PerFrameCB.h"
#include "PerCameraCB.h"

using namespace DirectX;

class MapRenderer
{
public:
    MapRenderer(ID3D11Device* device, ID3D11DeviceContext* deviceContext, InputManager* input_manager)
        : m_device(device)
        , m_deviceContext(deviceContext)
        , m_input_manager(input_manager)
    {
        m_mesh_manager = std::make_unique<MeshManager>(m_device, m_deviceContext);
        m_user_camera = std::make_unique<Camera>();
    }

    void Initialize(const float viewport_width, const float viewport_height)
    {
        // Initialize cameras
        float fov_degrees = 80.0f;
        float aspect_ratio = viewport_width / viewport_height;
        m_user_camera->SetFrustumAsPerspective(static_cast<float>(fov_degrees * XM_PI / 180.0), aspect_ratio,
                                               0.1f, 20000);
        const auto pos = FXMVECTOR{0, 0, -1000, 0};
        const auto target = FXMVECTOR{0, 0, 0, 0};
        const auto world_up = FXMVECTOR{0, 1, 0, 0};
        m_user_camera->LookAt(pos, target, world_up);

        m_input_manager->AddMouseMoveListener(m_user_camera.get());

        // Add a sphere at (0,0,0) in world coordinates. For testing the renderer.
        auto box_id = m_mesh_manager->AddBox({200, 200, 200});
        auto sphere_id = m_mesh_manager->AddSphere(300, 100, 100);
        auto line_id = m_mesh_manager->AddLine({-400, 500, 0}, {400, 500, 0});

        // Move the sphere and box next to eachother
        // Move the box to the left of the sphere (e.g., -250 units on the X-axis)
        DirectX::XMFLOAT4X4 boxWorldMatrix;
        DirectX::XMStoreFloat4x4(&boxWorldMatrix, DirectX::XMMatrixTranslation(-250, 0, 0));
        PerObjectCB boxPerObjectData;
        boxPerObjectData.world = boxWorldMatrix;
        m_mesh_manager->UpdateMeshPerObjectData(box_id, boxPerObjectData);

        // Move the sphere to the right of the box (e.g., 250 units on the X-axis)
        DirectX::XMFLOAT4X4 sphereWorldMatrix;
        DirectX::XMStoreFloat4x4(&sphereWorldMatrix, DirectX::XMMatrixTranslation(250, 0, 0));
        PerObjectCB spherePerObjectData;
        spherePerObjectData.world = sphereWorldMatrix;
        m_mesh_manager->UpdateMeshPerObjectData(sphere_id, spherePerObjectData);

        // Create and initialize the VertexShader
        m_vertex_shader = std::make_unique<VertexShader>(m_device, m_deviceContext);
        m_vertex_shader->Initialize(L"VertexShader.hlsl");

        // Create and initialize the PixelShader
        m_pixel_shader = std::make_unique<PixelShader>(m_device, m_deviceContext);
        m_pixel_shader->Initialize(L"PixelShader.hlsl");

        // Set up the constant buffer for the camera
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(PerFrameCB);
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&buffer_desc, nullptr, m_per_frame_cb.GetAddressOf());

        buffer_desc.ByteWidth = sizeof(PerCameraCB);
        m_device->CreateBuffer(&buffer_desc, nullptr, m_per_camera_cb.GetAddressOf());

        m_deviceContext->VSSetConstantBuffers(PER_FRAME_CB_SLOT, 1, m_per_frame_cb.GetAddressOf());
        m_deviceContext->VSSetConstantBuffers(PER_CAMERA_CB_SLOT, 1, m_per_camera_cb.GetAddressOf());

        m_deviceContext->VSSetShader(m_vertex_shader->GetShader(), nullptr, 0);
        m_deviceContext->VSSetSamplers(0, 1, m_pixel_shader->GetSamplerState());
        m_deviceContext->IASetInputLayout(m_vertex_shader->GetInputLayout());

        m_deviceContext->PSSetConstantBuffers(PER_FRAME_CB_SLOT, 1, m_per_frame_cb.GetAddressOf());
        m_deviceContext->PSSetConstantBuffers(PER_CAMERA_CB_SLOT, 1, m_per_camera_cb.GetAddressOf());

        // Set the pixel shader and sampler state
        m_deviceContext->PSSetShader(m_pixel_shader->GetShader(), nullptr, 0);
        m_deviceContext->PSSetSamplers(0, 1, m_pixel_shader->GetSamplerState());
    }

    void SetTerrain(const Mesh& terrain_mesh)
    {
        if (m_is_terrain_mesh_set)
        {
            m_mesh_manager->RemoveMesh(m_terrain_mesh_id);
            m_is_terrain_mesh_set = false;
        }

        m_terrain_mesh_id = m_mesh_manager->AddCustomMesh(terrain_mesh);
        m_is_terrain_mesh_set = true;
    }

    void UnsetTerrain()
    {
        if (m_is_terrain_mesh_set)
        {
            m_mesh_manager->RemoveMesh(m_terrain_mesh_id);
            m_is_terrain_mesh_set = false;
        }
    }

    void OnViewPortChanged(const float viewport_width, const float viewport_height)
    {
        m_user_camera->OnViewPortChanged(viewport_width, viewport_height);
    }

    void CreateRasterizerStates()
    {
        // Setup various rasterizer states
        D3D11_RASTERIZER_DESC rsDesc;
        ZeroMemory(&rsDesc, sizeof(D3D11_RASTERIZER_DESC));
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.FrontCounterClockwise = false;
        rsDesc.DepthClipEnable = true;

        auto hr = m_device->CreateRasterizerState(&rsDesc, m_solid_no_cull_rs.GetAddressOf());
        if (FAILED(hr))
            throw "Failed creating solid frame rasterizer state.";
        rsDesc.FillMode = D3D11_FILL_WIREFRAME;
        hr = m_device->CreateRasterizerState(&rsDesc, m_wireframe_no_cull_rs.GetAddressOf());
        if (FAILED(hr))
            throw "Failed creating solid frame rasterizer state.";
        rsDesc.CullMode = D3D11_CULL_BACK;
        hr = m_device->CreateRasterizerState(&rsDesc, m_wireframe_rs.GetAddressOf());
        if (FAILED(hr))
            throw "Failed creating solid frame rasterizer state.";
        rsDesc.FillMode = D3D11_FILL_SOLID;
        hr = m_device->CreateRasterizerState(&rsDesc, m_solid_rs.GetAddressOf());
        if (FAILED(hr))
            throw "Failed creating solid frame rasterizer state.";
    }

    void Update(const float dt)
    {
        // Walk
        if (m_input_manager->IsKeyDown('W'))
            m_user_camera->Walk(20, dt);
        if (m_input_manager->IsKeyDown('S'))
            m_user_camera->Walk(-20, dt);

        // Strafe
        if (m_input_manager->IsKeyDown('Q') || (m_input_manager->IsKeyDown('A')))
            m_user_camera->Strafe(-20, dt);
        if (m_input_manager->IsKeyDown('E') || (m_input_manager->IsKeyDown('D')))
            m_user_camera->Strafe(20, dt);

        m_user_camera->Update(dt);

        static DirectionalLight m_directionalLight;
        m_directionalLight.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
        m_directionalLight.diffuse = XMFLOAT4(0.6f, 0.5f, 0.5f, 1.0f);
        m_directionalLight.specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        m_directionalLight.direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
        m_directionalLight.pad = 0.0f;

        // Update per frame CB
        static auto frameCB = PerFrameCB();
        frameCB.directionalLight = m_directionalLight;

        // Update the per frame constant buffer
        D3D11_MAPPED_SUBRESOURCE mappedResourceFrame;
        ZeroMemory(&mappedResourceFrame, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_frame_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceFrame);
        memcpy(mappedResourceFrame.pData, &frameCB, sizeof(PerFrameCB));
        m_deviceContext->Unmap(m_per_frame_cb.Get(), 0);

        // Update camera CB
        static auto cameraCB = PerCameraCB();
        XMStoreFloat4x4(&cameraCB.view, XMMatrixTranspose(m_user_camera->GetView()));
        XMStoreFloat4x4(&cameraCB.projection, XMMatrixTranspose(m_user_camera->GetProj()));
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_camera_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &cameraCB, sizeof(PerCameraCB));
        m_deviceContext->Unmap(m_per_camera_cb.Get(), 0);

        m_mesh_manager->Update(dt);
    }

    void Render()
    {
        m_deviceContext->RSSetState(m_solid_rs.Get());
        m_mesh_manager->Render();
    }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    InputManager* m_input_manager;
    std::unique_ptr<MeshManager> m_mesh_manager;
    std::unique_ptr<Camera> m_user_camera;
    std::unique_ptr<VertexShader> m_vertex_shader;
    std::unique_ptr<PixelShader> m_pixel_shader;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_per_frame_cb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_per_camera_cb;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_wireframe_rs;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_wireframe_no_cull_rs;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_solid_rs;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_solid_no_cull_rs;

    bool m_is_terrain_mesh_set = false;
    int m_terrain_mesh_id = -1;
};