#pragma once
#include "InputManager.h"
#include "Camera.h"
#include "MeshManager.h"
#include "VertexShader.h"
#include "PerFrameCB.h"

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
        m_user_camera->Initialize(XMFLOAT3(1.0f, 2000.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
                                  XMFLOAT3(0.0f, 1.0f, 0.0f), fov_degrees * (XM_PI / 180.0f),
                                  viewport_width / viewport_height, 0.1, 1000);

        m_input_manager->AddMouseMoveListener(m_user_camera.get());

        // Add a sphere at (0,0,0) in world coordinates. For testing the renderer.
        m_mesh_manager->AddBox({100, 100, 100});

        // Create and initialize the VertexShader
        m_vertex_shader = std::make_unique<VertexShader>(m_device, m_deviceContext);
        m_vertex_shader->Initialize(L"VertexShader.hlsl");

        // Set up the constant buffer for the camera
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(PerFrameCB);
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&buffer_desc, nullptr, m_constant_buffer.GetAddressOf());

        m_deviceContext->VSSetConstantBuffers(PER_FRAME_CB_SLOT, 1, m_constant_buffer.GetAddressOf());

        // Assume shader and input layout stays the same.
        m_deviceContext->VSSetShader(m_vertex_shader->GetShader(), nullptr, 0);
        m_deviceContext->IASetInputLayout(m_vertex_shader->GetInputLayout());
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

    void Update(const float dt)
    {
        const bool is_w_down = m_input_manager->IsKeyDown('W');
        const bool is_a_down = m_input_manager->IsKeyDown('A');
        const bool is_s_down = m_input_manager->IsKeyDown('S');
        const bool is_d_down = m_input_manager->IsKeyDown('D');
        m_user_camera->Update(dt, is_a_down, is_w_down, is_s_down, is_d_down);

        m_mesh_manager->Update(dt);
    }

    void Render() { m_mesh_manager->Render(); }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    InputManager* m_input_manager;
    std::unique_ptr<MeshManager> m_mesh_manager;
    std::unique_ptr<Camera> m_user_camera;
    std::unique_ptr<VertexShader> m_vertex_shader;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constant_buffer;

    bool m_is_terrain_mesh_set = false;
    int m_terrain_mesh_id = -1;
};
