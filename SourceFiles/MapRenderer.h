#pragma once
#include "InputManager.h"
#include "Camera.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "PerFrameCB.h"
#include "PerCameraCB.h"
#include "PerTerrainCB.h"
#include "CheckerboardTexture.h"
#include "Terrain.h"
#include "BlendStateManager.h"
#include "RasterizerStateManager.h"
#include "DepthStencilStateManager.h"

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
        m_texture_manager = std::make_unique<TextureManager>(m_device, m_deviceContext);
        m_blend_state_manager = std::make_unique<BlendStateManager>(m_device, m_deviceContext);
        m_rasterizer_state_manager = std::make_unique<RasterizerStateManager>(m_device, m_deviceContext);
        m_stencil_state_manager = std::make_unique<DepthStencilStateManager>(m_device, m_deviceContext);
        m_user_camera = std::make_unique<Camera>();
    }

    void Initialize(const float viewport_width, const float viewport_height)
    {
        // Initialize cameras
        float fov_degrees = 70.0f;
        float aspect_ratio = viewport_width / viewport_height;
        m_user_camera->SetFrustumAsPerspective(static_cast<float>(fov_degrees * XM_PI / 180.0), aspect_ratio,
                                               0.1f, 200000);
        const auto pos = FXMVECTOR{0, 8500, 0, 0};
        const auto target = FXMVECTOR{1000, 6000, 1000, 0};
        const auto world_up = FXMVECTOR{0, 1, 0, 0};
        m_user_camera->LookAt(pos, target, world_up);

        m_input_manager->AddMouseMoveListener(m_user_camera.get());

        // Setup lighting
        m_directionalLight.ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        m_directionalLight.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        m_directionalLight.specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        m_directionalLight.direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
        m_directionalLight.pad = 0.0f;

        // Add a sphere at (0,0,0) in world coordinates. For testing the renderer.
        auto box_id = m_mesh_manager->AddBox({300, 300, 300});
        //auto box_mesh_instance = m_mesh_manager->GetMesh(box_id);
        auto sphere_id = m_mesh_manager->AddSphere(300, 100, 100);

        // Move the sphere and box next to eachother
        // Move the box to the left of the sphere (e.g., -250 units on the X-axis)
        DirectX::XMFLOAT4X4 boxWorldMatrix;
        DirectX::XMStoreFloat4x4(&boxWorldMatrix, DirectX::XMMatrixTranslation(30000, 0, 0));
        PerObjectCB boxPerObjectData;
        boxPerObjectData.world = boxWorldMatrix;
        boxPerObjectData.num_uv_texture_pairs = 1;
        m_mesh_manager->UpdateMeshPerObjectData(box_id, boxPerObjectData);

        // Move the sphere to the right of the box (e.g., 250 units on the X-axis)
        DirectX::XMFLOAT4X4 sphereWorldMatrix;
        DirectX::XMStoreFloat4x4(&sphereWorldMatrix, DirectX::XMMatrixTranslation(0, 0, 30000));
        PerObjectCB spherePerObjectData;
        spherePerObjectData.world = sphereWorldMatrix;
        m_mesh_manager->UpdateMeshPerObjectData(sphere_id, spherePerObjectData);

        int texture_width = 192;
        int texture_height = 192;
        int tile_size = 96;
        CheckerboardTexture checkerboard_texture(texture_width, texture_height, tile_size);
        auto texture_id =
          m_texture_manager->AddTexture((void*)checkerboard_texture.getData().data(), texture_width,
                                        texture_height, DXGI_FORMAT_R8G8B8A8_UNORM, 3214972);
        m_mesh_manager->SetTexturesForMesh(box_id, {m_texture_manager->GetTexture(texture_id)}, 0);

        // Create and initialize the VertexShader
        m_vertex_shader = std::make_unique<VertexShader>(m_device, m_deviceContext);
        m_vertex_shader->Initialize(L"VertexShader.hlsl");

        // Create and initialize the Default PixelShader
        m_pixel_shaders[PixelShaderType::Default] = std::make_unique<PixelShader>(m_device, m_deviceContext);
        m_pixel_shaders[PixelShaderType::Default]->Initialize(PixelShaderType::Default);

        if (! m_pixel_shaders.contains(PixelShaderType::TerrainTextured))
        {
            m_pixel_shaders[PixelShaderType::TerrainTextured] =
              std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainTextured]->Initialize(PixelShaderType::TerrainTextured);
        }

        if (! m_pixel_shaders.contains(PixelShaderType::TerrainCheckered))
        {
            m_pixel_shaders[PixelShaderType::TerrainCheckered] =
              std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainCheckered]->Initialize(PixelShaderType::TerrainCheckered);
        }

        if (! m_pixel_shaders.contains(PixelShaderType::TerrainDefault))
        {
            m_pixel_shaders[PixelShaderType::TerrainDefault] =
              std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainDefault]->Initialize(PixelShaderType::TerrainDefault);
        }

        // Set up the constant buffer for the camera
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth = sizeof(PerFrameCB);
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        m_device->CreateBuffer(&buffer_desc, nullptr, m_per_frame_cb.GetAddressOf());

        buffer_desc.ByteWidth = sizeof(PerCameraCB);
        m_device->CreateBuffer(&buffer_desc, nullptr, m_per_camera_cb.GetAddressOf());

        buffer_desc.ByteWidth = sizeof(PerTerrainCB);
        m_device->CreateBuffer(&buffer_desc, nullptr, m_per_terrain_cb.GetAddressOf());

        //
        m_deviceContext->VSSetConstantBuffers(PER_FRAME_CB_SLOT, 1, m_per_frame_cb.GetAddressOf());
        m_deviceContext->VSSetConstantBuffers(PER_CAMERA_CB_SLOT, 1, m_per_camera_cb.GetAddressOf());
        m_deviceContext->VSSetConstantBuffers(PER_TERRAIN_CB_SLOT, 1, m_per_terrain_cb.GetAddressOf());

        m_deviceContext->VSSetShader(m_vertex_shader->GetShader(), nullptr, 0);
        m_deviceContext->IASetInputLayout(m_vertex_shader->GetInputLayout());

        m_deviceContext->PSSetConstantBuffers(PER_FRAME_CB_SLOT, 1, m_per_frame_cb.GetAddressOf());
        m_deviceContext->PSSetConstantBuffers(PER_CAMERA_CB_SLOT, 1, m_per_camera_cb.GetAddressOf());
        m_deviceContext->PSSetConstantBuffers(PER_TERRAIN_CB_SLOT, 1, m_per_terrain_cb.GetAddressOf());

        // Set the pixel shader and sampler state
        m_deviceContext->PSSetSamplers(0, 1, m_pixel_shaders[PixelShaderType::Default]->GetSamplerState());
        m_deviceContext->PSSetShader(m_pixel_shaders[PixelShaderType::Default]->GetShader(), nullptr, 0);
    }

    Camera* GetCamera() { return m_user_camera.get(); }

    void SetFrustumAsPerspective(float fovY, float aspectRatio, float zNear, float zFar)
    {
        m_user_camera->SetFrustumAsPerspective(fovY, aspectRatio, zNear, zFar);
    }
    void SetFrustumAsOrthographic(float view_width, float view_height, float near_z, float far_z)
    {
        m_user_camera->SetFrustumAsOrthographic(view_width, view_height, near_z, far_z);
    }

    void SetDirectionalLight(DirectionalLight new_directional_light)
    {
        m_directionalLight = new_directional_light;
        m_per_frame_cb_changed = true;
    }
    const DirectionalLight GetDirectionalLight() { return m_directionalLight; }

    void UpdateTerrainWaterLevel(float new_water_level)
    {
        auto cb = m_terrain->m_per_terrain_cb;
        cb.water_level = new_water_level;
        m_terrain->m_per_terrain_cb = cb;

        D3D11_MAPPED_SUBRESOURCE mappedResourceFrame;
        ZeroMemory(&mappedResourceFrame, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_terrain_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceFrame);
        memcpy(mappedResourceFrame.pData, &m_terrain->m_per_terrain_cb, sizeof(PerTerrainCB));
        m_deviceContext->Unmap(m_per_terrain_cb.Get(), 0);
    }

    Terrain* GetTerrain() { return m_terrain.get(); }

    void SetTerrain(std::unique_ptr<Terrain> terrain, int texture_atlas_id)
    {
        if (m_is_terrain_mesh_set)
        {
            UnsetTerrain();
        }

        auto mesh = terrain->get_mesh();
        mesh->num_textures = 1;
        m_terrain_mesh_id = m_mesh_manager->AddCustomMesh(mesh, m_terrain_current_pixel_shader_type);
        m_terrain_texture_atlas_id = texture_atlas_id;

        PerObjectCB terrainPerObjectData;
        terrainPerObjectData.num_uv_texture_pairs = mesh->num_textures;
        for (int i = 0; i < mesh->uv_coord_indices.size(); i++)
        {
            int index0 = i / 4;
            int index1 = i % 4;

            terrainPerObjectData.uv_indices[index0][index1] = (uint32_t)mesh->uv_coord_indices[i];
            terrainPerObjectData.texture_indices[index0][index1] = (uint32_t)mesh->tex_indices[i];
            terrainPerObjectData.blend_flags[index0][index1] = (uint32_t)mesh->blend_flags[i];
        }
        m_mesh_manager->UpdateMeshPerObjectData(m_terrain_mesh_id, terrainPerObjectData);

        auto water_level = m_terrain ? m_terrain->m_per_terrain_cb.water_level : 0.0f;
        terrain->m_per_terrain_cb =
          PerTerrainCB(terrain->m_grid_dim_x, terrain->m_grid_dim_z, terrain->m_bounds.map_min_x,
                       terrain->m_bounds.map_max_x, terrain->m_bounds.map_min_y, terrain->m_bounds.map_max_y,
                       terrain->m_bounds.map_min_z, terrain->m_bounds.map_max_z, water_level, {0, 0, 0});

        D3D11_MAPPED_SUBRESOURCE mappedResourceFrame;
        ZeroMemory(&mappedResourceFrame, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_terrain_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceFrame);
        memcpy(mappedResourceFrame.pData, &terrain->m_per_terrain_cb, sizeof(PerTerrainCB));
        m_deviceContext->Unmap(m_per_terrain_cb.Get(), 0);

        int texture_id = 0;
        if (! m_terrain)
        {
            // Create and set texture. Just make it 2x2 checkered tiles. It will be repeated in the pixel shader.
            int texture_tile_size = 96;
            int texture_width = texture_tile_size * 2;
            int texture_height = texture_tile_size * 2;
            CheckerboardTexture checkerboard_texture(texture_width, texture_height, texture_tile_size);
            m_terrain_checkered_texture_id =
              m_texture_manager->AddTexture((void*)checkerboard_texture.getData().data(), texture_width,
                                            texture_height, DXGI_FORMAT_R8G8B8A8_UNORM, -1);
        }

        if (m_terrain_current_pixel_shader_type == PixelShaderType::TerrainCheckered ||
            m_terrain_texture_atlas_id < 0)
        {
            m_mesh_manager->SetTexturesForMesh(
              m_terrain_mesh_id, {m_texture_manager->GetTexture(m_terrain_checkered_texture_id)}, 0);
        }
        else
        {
            m_mesh_manager->SetTexturesForMesh(
              m_terrain_mesh_id, {m_texture_manager->GetTexture(m_terrain_texture_atlas_id)}, 0);
        }

        // Now we create a texture used for splatting (blending terrain textures)
        const auto& texture_index_grid = terrain->get_texture_index_grid();
        const auto& texture_blend_weights_grid = terrain->get_texture_blend_weights_grid();

        // Create a 2D texture from the texture_index_grid
        int texture_width = terrain->m_grid_dim_x;
        int texture_height = terrain->m_grid_dim_z;
        std::vector<uint8_t> terain_texture_data(texture_width * texture_height);
        std::vector<uint8_t> terain_texture_blend_weights_data(texture_width * texture_height);
        for (int i = 0; i < texture_height; ++i)
        {
            for (int j = 0; j < texture_width; ++j)
            {
                terain_texture_data[i * texture_width + j] = texture_index_grid[i][j];
                terain_texture_blend_weights_data[i * texture_width + j] = texture_blend_weights_grid[i][j];
            }
        }

        // Create the texture and add it to the texture manager
        m_terrain_texture_indices_id = m_texture_manager->AddTexture(
          terain_texture_data.data(), texture_width, texture_height, DXGI_FORMAT_R8_UNORM, -1);

        m_terrain_texture_blend_weights_id = m_texture_manager->AddTexture(
          terain_texture_blend_weights_data.data(), texture_width, texture_height, DXGI_FORMAT_R8_UNORM, -1);

        m_mesh_manager->SetTexturesForMesh(m_terrain_mesh_id,
                                           {m_texture_manager->GetTexture(m_terrain_texture_indices_id)}, 1);
        m_mesh_manager->SetTexturesForMesh(
          m_terrain_mesh_id, {m_texture_manager->GetTexture(m_terrain_texture_blend_weights_id)}, 2);

        m_terrain = std::move(terrain);
        m_is_terrain_mesh_set = true;
    }

    void UnsetTerrain()
    {
        if (m_is_terrain_mesh_set)
        {
            m_mesh_manager->RemoveMesh(m_terrain_mesh_id);
            m_is_terrain_mesh_set = false;
        }

        for (const auto& [model_id, model_prop_ids] : m_prop_mesh_ids)
        {
            for (const auto mesh_id : model_prop_ids)
            {
                m_mesh_manager->RemoveMesh(mesh_id);
            }
        }

        m_prop_mesh_ids.clear();
    }

    // A prop consists of 1+ sub models/meshes.
    std::vector<int> AddProp(std::vector<Mesh> meshes, std::vector<PerObjectCB> per_object_cbs,
                             uint32_t model_id)
    {
        std::vector<int> mesh_ids;
        for (int i = 0; i < meshes.size(); i++)
        {
            const auto& mesh = meshes[i];
            const auto& per_object_cb = per_object_cbs[i];

            int mesh_id = m_mesh_manager->AddCustomMesh(mesh);
            m_mesh_manager->UpdateMeshPerObjectData(mesh_id, per_object_cb);
            mesh_ids.push_back(mesh_id);
        }

        m_prop_mesh_ids.push_back({model_id, mesh_ids});

        return mesh_ids;
    }

    std::vector<std::pair<uint32_t, std::vector<int>>>& GetPropsMeshIds() { return m_prop_mesh_ids; }

    PixelShaderType GetTerrainPixelShaderType() { return m_terrain_current_pixel_shader_type; }
    void SetTerrainPixelShaderType(PixelShaderType pixel_shader_type)
    {
        if (m_is_terrain_mesh_set)
        {
            if (pixel_shader_type == PixelShaderType::TerrainCheckered && m_terrain_checkered_texture_id >= 0)
            {
                m_mesh_manager->ChangeMeshPixelShaderType(m_terrain_mesh_id, pixel_shader_type);
                m_mesh_manager->SetTexturesForMesh(
                  m_terrain_mesh_id, {m_texture_manager->GetTexture(m_terrain_checkered_texture_id)}, 0);

                m_terrain_current_pixel_shader_type = pixel_shader_type;
            }
            else if (pixel_shader_type == PixelShaderType::TerrainTextured && m_terrain_texture_atlas_id >= 0)
            {
                m_mesh_manager->ChangeMeshPixelShaderType(m_terrain_mesh_id, pixel_shader_type);
                m_mesh_manager->SetTexturesForMesh(
                  m_terrain_mesh_id, {m_texture_manager->GetTexture(m_terrain_texture_atlas_id)}, 0);

                m_terrain_current_pixel_shader_type = pixel_shader_type;
            }
        }
    }

    void OnViewPortChanged(const float viewport_width, const float viewport_height)
    {
        m_user_camera->OnViewPortChanged(viewport_width, viewport_height);
    }

    RasterizerStateType GetCurrentRasterizerState()
    {
        return m_rasterizer_state_manager->GetCurrentRasterizerState();
    }
    void SwitchRasterizerState(RasterizerStateType state)
    {
        m_rasterizer_state_manager->SetRasterizerState(state);
    }

    void SetMeshShouldRender(int mesh_id, bool should_render)
    {
        m_mesh_manager->SetMeshShouldRender(mesh_id, should_render);
    }

    bool GetMeshShouldRender(int mesh_id) const { return m_mesh_manager->GetMeshShouldRender(mesh_id); }

    TextureManager* GetTextureManager() { return m_texture_manager.get(); }
    MeshManager* GetMeshManager() { return m_mesh_manager.get(); }

    void Update(const float dt)
    {
        // Walk
        if (m_input_manager->IsKeyDown('W'))
            m_user_camera->Walk(WalkDirection::Forward, dt);
        if (m_input_manager->IsKeyDown('S'))
            m_user_camera->Walk(WalkDirection::Backward, dt);

        // Strafe
        if (m_input_manager->IsKeyDown('Q') || (m_input_manager->IsKeyDown('A')))
            m_user_camera->Strafe(StrafeDirection::Left, dt);
        if (m_input_manager->IsKeyDown('E') || (m_input_manager->IsKeyDown('D')))
            m_user_camera->Strafe(StrafeDirection::Right, dt);

        m_user_camera->Update(dt);

        // Update per frame CB
        if (m_per_frame_cb_changed)
        {
            PerFrameCB frameCB;
            frameCB.directionalLight = m_directionalLight;

            // Update the per frame constant buffer
            D3D11_MAPPED_SUBRESOURCE mappedResourceFrame;
            ZeroMemory(&mappedResourceFrame, sizeof(D3D11_MAPPED_SUBRESOURCE));
            m_deviceContext->Map(m_per_frame_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceFrame);
            memcpy(mappedResourceFrame.pData, &frameCB, sizeof(PerFrameCB));
            m_deviceContext->Unmap(m_per_frame_cb.Get(), 0);
        }

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
        m_mesh_manager->Render(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                               m_stencil_state_manager.get());
    }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    InputManager* m_input_manager;
    std::unique_ptr<MeshManager> m_mesh_manager;
    std::unique_ptr<TextureManager> m_texture_manager;
    std::unique_ptr<BlendStateManager> m_blend_state_manager;
    std::unique_ptr<RasterizerStateManager> m_rasterizer_state_manager;
    std::unique_ptr<DepthStencilStateManager> m_stencil_state_manager;
    std::unique_ptr<Camera> m_user_camera;
    std::unique_ptr<VertexShader> m_vertex_shader;
    std::unordered_map<PixelShaderType, std::unique_ptr<PixelShader>> m_pixel_shaders;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_per_frame_cb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_per_camera_cb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_per_terrain_cb;

    std::unique_ptr<Terrain> m_terrain;
    PixelShaderType m_terrain_current_pixel_shader_type = PixelShaderType::TerrainTextured;

    std::vector<std::pair<uint32_t, std::vector<int>>> m_prop_mesh_ids;

    bool m_is_terrain_mesh_set = false;
    int m_terrain_mesh_id = -1;
    int m_terrain_checkered_texture_id = -1;
    int m_terrain_texture_indices_id = -1;
    int m_terrain_texture_blend_weights_id = -1;
    int m_terrain_texture_atlas_id = -1;

    DirectionalLight m_directionalLight;
    bool m_per_frame_cb_changed = true;
};
