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
#include "DeviceResources.h"

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
        float fov_degrees = 60.0f;
        float aspect_ratio = viewport_width / viewport_height;
        m_user_camera->SetFrustumAsPerspective(static_cast<float>(fov_degrees * XM_PI / 180.0), aspect_ratio,
            10.0f, 200000);
        const auto pos = FXMVECTOR{ 0, 8500, 0, 0 };
        const auto target = FXMVECTOR{ 1000, 6000, 1000, 0 };
        const auto world_up = FXMVECTOR{ 0, 1, 0, 0 };
        m_user_camera->LookAt(pos, target, world_up);

        m_input_manager->AddMouseMoveListener(m_user_camera.get());

        // Setup lighting
        m_directionalLight.ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        m_directionalLight.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        m_directionalLight.specular = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
        m_directionalLight.direction = XMFLOAT3(1.0f, -0.98f, 0.0f);
        m_directionalLight.pad = 0.0f;

        // Create and initialize the VertexShader
        m_vertex_shader = std::make_unique<VertexShader>(m_device, m_deviceContext);
        m_vertex_shader->Initialize(L"VertexShader.hlsl");

        // Create and initialize the Pixel Shaders
        if (!m_pixel_shaders.contains(PixelShaderType::OldModel))
        {
            m_pixel_shaders[PixelShaderType::OldModel] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::OldModel]->Initialize(PixelShaderType::OldModel);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::NewModel))
        {
            m_pixel_shaders[PixelShaderType::NewModel] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::NewModel]->Initialize(PixelShaderType::NewModel);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::PickingShader))
        {
            m_pixel_shaders[PixelShaderType::PickingShader] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::PickingShader]->Initialize(PixelShaderType::PickingShader);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::TerrainTexturedWithShadows))
        {
            m_pixel_shaders[PixelShaderType::TerrainTexturedWithShadows] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainTexturedWithShadows]->Initialize(PixelShaderType::TerrainTexturedWithShadows);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::TerrainRev))
        {
            m_pixel_shaders[PixelShaderType::TerrainRev] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainRev]->Initialize(PixelShaderType::TerrainRev);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::TerrainTextured))
        {
            m_pixel_shaders[PixelShaderType::TerrainTextured] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainTextured]->Initialize(PixelShaderType::TerrainTextured);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::TerrainCheckered))
        {
            m_pixel_shaders[PixelShaderType::TerrainCheckered] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainCheckered]->Initialize(PixelShaderType::TerrainCheckered);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::TerrainDefault))
        {
            m_pixel_shaders[PixelShaderType::TerrainDefault] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainDefault]->Initialize(PixelShaderType::TerrainDefault);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::Sky))
        {
            m_pixel_shaders[PixelShaderType::Sky] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::Sky]->Initialize(PixelShaderType::Sky);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::Clouds))
        {
            m_pixel_shaders[PixelShaderType::Clouds] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::Clouds]->Initialize(PixelShaderType::Clouds);
        }
        
        if (!m_pixel_shaders.contains(PixelShaderType::Water))
        {
            m_pixel_shaders[PixelShaderType::Water] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::Water]->Initialize(PixelShaderType::Water);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::Shore))
        {
            m_pixel_shaders[PixelShaderType::Shore] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::Shore]->Initialize(PixelShaderType::Shore);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::OldModelShadowMap))
        {
            m_pixel_shaders[PixelShaderType::OldModelShadowMap] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::OldModelShadowMap]->Initialize(PixelShaderType::OldModelShadowMap);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::OldModelReflection))
        {
            m_pixel_shaders[PixelShaderType::OldModelReflection] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::OldModelReflection]->Initialize(PixelShaderType::OldModelReflection);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::NewModelShadowMap))
        {
            m_pixel_shaders[PixelShaderType::NewModelShadowMap] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::NewModelShadowMap]->Initialize(PixelShaderType::NewModelShadowMap);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::NewModelReflection))
        {
            m_pixel_shaders[PixelShaderType::NewModelReflection] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::NewModelReflection]->Initialize(PixelShaderType::NewModelReflection);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::TerrainReflectionTexturedWithShadows))
        {
            m_pixel_shaders[PixelShaderType::TerrainReflectionTexturedWithShadows] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainReflectionTexturedWithShadows]->Initialize(PixelShaderType::TerrainReflectionTexturedWithShadows);
        }

        if (!m_pixel_shaders.contains(PixelShaderType::TerrainShadowMap))
        {
            m_pixel_shaders[PixelShaderType::TerrainShadowMap] =
                std::make_unique<PixelShader>(m_device, m_deviceContext);
            m_pixel_shaders[PixelShaderType::TerrainShadowMap]->Initialize(PixelShaderType::TerrainShadowMap);
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
        m_deviceContext->PSSetSamplers(0, 1, m_pixel_shaders[PixelShaderType::OldModel]->GetSamplerState());
        m_deviceContext->PSSetSamplers(1, 1, m_pixel_shaders[PixelShaderType::OldModel]->GetSamplerStateShadow());

        m_deviceContext->PSSetShader(m_pixel_shaders[PixelShaderType::OldModel]->GetShader(), nullptr, 0);
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
        // Compare the new direction with the current direction
        if (m_directionalLight.direction.x != new_directional_light.direction.x ||
            m_directionalLight.direction.y != new_directional_light.direction.y ||
            m_directionalLight.direction.z != new_directional_light.direction.z)
        {
            m_should_rerender_shadows = true; // Set to true only if the direction has changed
        }

        m_directionalLight = new_directional_light;
        m_per_frame_cb_changed = true;
    }


    const DirectionalLight GetDirectionalLight() { return m_directionalLight; }

    void UpdateTerrainWaterLevel(float new_water_level)
    {
        m_water_level = new_water_level;
        auto cb = m_terrain->m_per_terrain_cb;
        cb.water_level = m_water_level;
        m_terrain->m_per_terrain_cb = cb;

        D3D11_MAPPED_SUBRESOURCE mappedResourceFrame;
        ZeroMemory(&mappedResourceFrame, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_terrain_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceFrame);
        memcpy(mappedResourceFrame.pData, &m_terrain->m_per_terrain_cb, sizeof(PerTerrainCB));
        m_deviceContext->Unmap(m_per_terrain_cb.Get(), 0);
    }

    float GetWaterLevel() { return m_water_level; }

    void UpdateTerrainTexturePadding(float padding_x, float padding_y)
    {
        auto cb = m_terrain->m_per_terrain_cb;
        cb.terrain_texture_pad_x = padding_x;
        cb.terrain_texture_pad_y = padding_y;
        m_terrain->m_per_terrain_cb = cb;

        D3D11_MAPPED_SUBRESOURCE mappedResourceFrame;
        ZeroMemory(&mappedResourceFrame, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_terrain_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceFrame);
        memcpy(mappedResourceFrame.pData, &m_terrain->m_per_terrain_cb, sizeof(PerTerrainCB));
        m_deviceContext->Unmap(m_per_terrain_cb.Get(), 0);
    }

    void UpdateWaterProperties(
        float water_distortion_tex_scale,
        float water_distortion_scale,
        float water_distortion_tex_speed,
        float water_color_tex_scale,
        float water_color_tex_speed,
        DirectX::XMFLOAT4 color0,
        DirectX::XMFLOAT4 color1
    )
    {
        auto& cb = m_terrain->m_per_terrain_cb;

        // Update new fields
        cb.water_distortion_tex_scale = water_distortion_tex_scale;
        cb.water_distortion_scale = water_distortion_scale;
        cb.water_distortion_tex_speed = water_distortion_tex_speed;
        cb.water_color_tex_scale = water_color_tex_scale;
        cb.water_color_tex_speed = water_color_tex_speed;
        cb.color0 = color0;
        cb.color1 = color1;

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
        HRESULT hr = m_deviceContext->Map(m_per_terrain_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            memcpy(mappedResource.pData, &cb, sizeof(PerTerrainCB));
            m_deviceContext->Unmap(m_per_terrain_cb.Get(), 0);
        }
    }


    Terrain* GetTerrain() { return m_terrain; }

    void SetTerrain(Terrain* terrain, int texture_atlas_id)
    {
        m_should_rerender_shadows = true;
        if (m_is_terrain_mesh_set)
        {
            UnsetTerrain();
        }

        auto mesh = terrain->get_mesh();
        mesh->num_textures = 1;
        m_terrain_mesh_id = m_mesh_manager->AddCustomMesh(mesh, m_terrain_current_pixel_shader_type);
        m_mesh_manager->SetMeshShouldRender(m_terrain_mesh_id, false); // We'll render it manually.
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
            terrainPerObjectData.texture_types[index0][index1] = (uint32_t)mesh->texture_types[i];
        }
        m_mesh_manager->UpdateMeshPerObjectData(m_terrain_mesh_id, terrainPerObjectData);

        terrain->m_per_terrain_cb =
            PerTerrainCB(terrain->m_grid_dim_x, terrain->m_grid_dim_z, terrain->m_bounds.map_min_x,
                terrain->m_bounds.map_max_x, terrain->m_bounds.map_min_y, terrain->m_bounds.map_max_y,
                terrain->m_bounds.map_min_z, terrain->m_bounds.map_max_z, m_water_level, 0.03, 0.03, { 0 });

        D3D11_MAPPED_SUBRESOURCE mappedResourceFrame;
        ZeroMemory(&mappedResourceFrame, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_terrain_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceFrame);
        memcpy(mappedResourceFrame.pData, &terrain->m_per_terrain_cb, sizeof(PerTerrainCB));
        m_deviceContext->Unmap(m_per_terrain_cb.Get(), 0);

        // Create checkered texture so the user can choose to render the terrain with the checkered pixel shader.
        int texture_tile_size = 96;
        int texture_width = texture_tile_size * 2;
        int texture_height = texture_tile_size * 2;
        auto color_choice1 = CheckerboardTexture::ColorChoice::White;
        auto color_choice2 = CheckerboardTexture::ColorChoice::DimGray;
        CheckerboardTexture checkerboard_texture(texture_width, texture_height, texture_tile_size, color_choice1, color_choice2, 1.0f);
        m_terrain_checkered_texture_id =
            m_texture_manager->AddTexture((void*)checkerboard_texture.getData().data(), texture_width,
                texture_height, DXGI_FORMAT_R8G8B8A8_UNORM, 3214972 + (int)color_choice1 * 20 + (int)color_choice2);

        if (m_terrain_current_pixel_shader_type == PixelShaderType::TerrainCheckered ||
            m_terrain_texture_atlas_id < 0)
        {
            m_mesh_manager->SetTexturesForMesh(
                m_terrain_mesh_id, { m_texture_manager->GetTexture(m_terrain_checkered_texture_id) }, 0);
        }
        else
        {
            m_mesh_manager->SetTexturesForMesh(
                m_terrain_mesh_id, { m_texture_manager->GetTexture(m_terrain_texture_atlas_id) }, 0);
        }

        // Now we create a texture used for splatting (blending terrain textures)
        const auto& texture_index_grid = terrain->get_texture_index_grid();
        const auto& terrain_shadow_map_grid = terrain->get_terrain_shadow_map_grid();

        // Create a 2D texture from the texture_index_grid
        texture_width = terrain->m_grid_dim_x;
        texture_height = terrain->m_grid_dim_z;
        std::vector<uint8_t> terain_texture_data(texture_width * texture_height);
        std::vector<uint8_t> terrain_shadow_map_data(texture_width * texture_height);
        for (int i = 0; i < texture_height; ++i)
        {
            for (int j = 0; j < texture_width; ++j)
            {
                terain_texture_data[i * texture_width + j] = texture_index_grid[i][j];
                terrain_shadow_map_data[i * texture_width + j] = terrain_shadow_map_grid[i][j];
            }
        }

        // Create the texture and add it to the texture manager
        m_terrain_texture_indices_id = m_texture_manager->AddTexture(
            terain_texture_data.data(), texture_width, texture_height, DXGI_FORMAT_R8_UNORM, -1);

        m_terrain_shadow_map_id = m_texture_manager->AddTexture(
            terrain_shadow_map_data.data(), texture_width, texture_height, DXGI_FORMAT_R8_UNORM, -1);

        m_mesh_manager->SetTexturesForMesh(m_terrain_mesh_id,
            { m_texture_manager->GetTexture(m_terrain_texture_indices_id) }, 1);
        m_mesh_manager->SetTexturesForMesh(
            m_terrain_mesh_id, { m_texture_manager->GetTexture(m_terrain_shadow_map_id) }, 2);

        m_terrain = terrain;
        m_is_terrain_mesh_set = true;
    }

    void UnsetTerrain()
    {
        if (m_is_terrain_mesh_set)
        {
            m_mesh_manager->RemoveMesh(m_terrain_mesh_id);
            m_is_terrain_mesh_set = false;
            m_terrain = nullptr;
        }

        for (const auto& [model_id, model_prop_ids] : m_prop_mesh_ids)
        {
            for (const auto mesh_id : model_prop_ids)
            {
                m_mesh_manager->RemoveMesh(mesh_id);
            }
        }

        for (const auto& mesh_id : extra_mesh_ids) {
            m_mesh_manager->RemoveMesh(mesh_id);
        }

        m_prop_mesh_ids.clear();
    }

    void SetTerrainMeshId(int terrain_mesh_id) { m_terrain_mesh_id = terrain_mesh_id; }
    int GetTerrainMeshId() { return m_terrain_mesh_id; }
    
    void SetSkyMeshId(int sky_mesh_id) { m_sky_mesh_id = sky_mesh_id; }
    int GetSkyMeshId() { return m_sky_mesh_id; }

    void SetCloudsMeshId(int clouds_mesh_id) { m_clouds_mesh_id = clouds_mesh_id; }
    int GetCloudsMeshId() { return m_clouds_mesh_id; }

    void SetWaterMeshId(int water_mesh_id) { m_water_mesh_id = water_mesh_id; }
    int GetWaterMeshId() { return m_water_mesh_id; }

    // A prop consists of 1+ sub models/meshes.
    std::vector<int> AddProp(std::vector<Mesh> meshes, std::vector<PerObjectCB>& per_object_cbs,
        uint32_t model_id, PixelShaderType pixel_shader_type)
    {
        m_should_rerender_shadows = true;
        std::vector<int> mesh_ids;
        for (int i = 0; i < meshes.size(); i++)
        {
            const auto& mesh = meshes[i];
            auto& per_object_cb = per_object_cbs[i];

            int mesh_id = m_mesh_manager->AddCustomMesh(mesh, pixel_shader_type);
            per_object_cb.object_id = mesh_id;
            m_mesh_manager->UpdateMeshPerObjectData(mesh_id, per_object_cb);
            mesh_ids.push_back(mesh_id);
        }

        const auto it = m_prop_mesh_ids.find(model_id);
        if (it != m_prop_mesh_ids.end()) {
            it->second.insert(it->second.end(), mesh_ids.begin(), mesh_ids.end());
        }
        else {
            m_prop_mesh_ids.emplace(model_id, mesh_ids);
        }

        return mesh_ids;
    }

    void ClearProps() {
        m_should_rerender_shadows = true;
        for (const auto mesh_ids : m_prop_mesh_ids) {
            for (const auto mesh_id : mesh_ids.second) {
                m_mesh_manager->RemoveMesh(mesh_id);
            }
        }

        m_prop_mesh_ids.clear();
    }

    void SetShore(std::vector<Mesh>& shore_meshes, std::vector<ID3D11ShaderResourceView*> textures, std::vector<PerObjectCB>& object_cbs, PixelShaderType pixel_shader_type)
    {
        m_shore_mesh_ids.clear();
        should_render_shore_mesh_id.clear();

        for (int i = 0; i < shore_meshes.size(); i++)
        {
            const auto& mesh = shore_meshes[i];

            int mesh_id = m_mesh_manager->AddCustomMesh(mesh, pixel_shader_type);
            m_mesh_manager->SetTexturesForMesh(mesh_id, textures, 3);
            m_mesh_manager->UpdateMeshPerObjectData(mesh_id, object_cbs[i]);
            SetMeshShouldRender(mesh_id, false); // don't render with props. Will only render when called "manually".
            m_shore_mesh_ids.push_back(mesh_id);
            should_render_shore_mesh_id.insert({ mesh_id, true }); // this is how we can select individually which shore meshes to render.
        }
    }

    void SetShoreMeshIdShouldRender(int mesh_id, bool should_render) {
        auto it = should_render_shore_mesh_id.find(mesh_id);
        if (it != should_render_shore_mesh_id.end()) {
            it->second = should_render;
        }
    }

    bool GetShoreMeshIdShouldRender(int mesh_id) {
        auto it = should_render_shore_mesh_id.find(mesh_id);
        if (it != should_render_shore_mesh_id.end()) {
            return it->second;
        }

        return false;
    }

    int GetObjectId(ID3D11Texture2D* picking_target, const int x, const int y) const
    {
        return m_mesh_manager->GetPickedObjectId(m_deviceContext, picking_target, x, y);
    }

    std::map<uint32_t, std::vector<int>>& GetPropsMeshIds() { return m_prop_mesh_ids; }
    std::vector<int>& GetShoreMeshIds() { return m_shore_mesh_ids; }

    PixelShaderType GetTerrainPixelShaderType() { return m_terrain_current_pixel_shader_type; }
    void SetTerrainPixelShaderType(PixelShaderType pixel_shader_type)
    {
        if (m_is_terrain_mesh_set)
        {
            if (pixel_shader_type == PixelShaderType::TerrainCheckered && m_terrain_checkered_texture_id >= 0)
            {
                m_mesh_manager->ChangeMeshPixelShaderType(m_terrain_mesh_id, pixel_shader_type);
                m_mesh_manager->SetTexturesForMesh(
                    m_terrain_mesh_id, { m_texture_manager->GetTexture(m_terrain_checkered_texture_id) }, 0);

                m_terrain_current_pixel_shader_type = pixel_shader_type;
            }
            else if (pixel_shader_type == PixelShaderType::TerrainTextured)
            {
                m_mesh_manager->ChangeMeshPixelShaderType(m_terrain_mesh_id, pixel_shader_type);
                m_mesh_manager->SetTexturesForMesh(
                    m_terrain_mesh_id, { m_texture_manager->GetTexture(m_terrain_texture_atlas_id) }, 0);

                m_terrain_current_pixel_shader_type = pixel_shader_type;
            }
            else if (pixel_shader_type == PixelShaderType::TerrainTexturedWithShadows)
            {
                m_mesh_manager->ChangeMeshPixelShaderType(m_terrain_mesh_id, pixel_shader_type);
                m_mesh_manager->SetTexturesForMesh(
                    m_terrain_mesh_id, { m_texture_manager->GetTexture(m_terrain_texture_atlas_id) }, 0);

                m_terrain_current_pixel_shader_type = pixel_shader_type;
            }
            else if (pixel_shader_type == PixelShaderType::TerrainRev)
            {
                m_mesh_manager->ChangeMeshPixelShaderType(m_terrain_mesh_id, pixel_shader_type);
                m_mesh_manager->SetTexturesForMesh(
                    m_terrain_mesh_id, { m_texture_manager->GetTexture(m_terrain_texture_atlas_id) }, 0);

                m_terrain_current_pixel_shader_type = pixel_shader_type;
            }
            else
            {
                m_mesh_manager->ChangeMeshPixelShaderType(m_terrain_mesh_id, PixelShaderType::TerrainDefault);

                m_terrain_current_pixel_shader_type = PixelShaderType::TerrainDefault;
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
        m_should_rerender_shadows = true;
        m_mesh_manager->SetMeshShouldRender(mesh_id, should_render);
    }

    bool GetMeshShouldRender(int mesh_id) const { return m_mesh_manager->GetMeshShouldRender(mesh_id); }

    TextureManager* GetTextureManager() { return m_texture_manager.get(); }
    MeshManager* GetMeshManager() { return m_mesh_manager.get(); }

    void SetLODQuality(const LODQuality new_lod_quality) {
        m_lod_quality = new_lod_quality;
    }

    LODQuality GetLODQuality() const {
        return m_lod_quality;
    }

    void SetShouldRenderSky(bool should_render_sky) { m_should_render_sky = should_render_sky; }
    bool GetShouldRenderSky() { return m_should_render_sky; }

    void SetSkyHeight(float sky_height) { m_sky_height = sky_height; }
    float GetSkyHeight() { return m_sky_height; }

    void SetClearColor(XMFLOAT4 clear_color) { m_clear_color = clear_color; }
    XMFLOAT4& GetClearColor() { return m_clear_color; }

    void SetFogStart(float fog_start) { m_fog_start = fog_start; }
    void SetFogEnd(float fog_end) { m_fog_end = fog_end; }
    void SetFogStartY(float fog_start_y) { m_fog_start_y = fog_start_y; }
    void SetFogEndY(float fog_end_y) { m_fog_end_y = fog_end_y; }

    float GetFogStart() const { return m_fog_start; }
    float GetFogEnd() const { return m_fog_end; }
    float GetFogStartY() const { return m_fog_start_y; }
    float GetFogEndY() const { return m_fog_end_y; }

    void SetShouldRenderShadows(bool should_render_shadows) { m_should_render_shadows = should_render_shadows; }
    void SetShouldRenderWaterReflection(bool should_render_reflection) { m_should_render_water_reflection = should_render_reflection; }
    void SetShouldRenderFog(bool should_render_fog) { m_should_render_fog = should_render_fog; }
    void SetShouldRerenderShadows(bool should_rerender_shadows) { m_should_rerender_shadows = should_rerender_shadows; }
    void SetShouldRenderShadowsForModels(bool should_render_shadows) { m_should_render_shadows_for_models = should_render_shadows; }

    bool GetShouldRenderShadows() { return m_should_render_shadows; }
    bool GetShouldRenderWaterReflection() { return m_should_render_water_reflection; }
    bool GetShouldRenderFog() { return m_should_render_fog; }
    bool GetShouldRerenderShadows() { return m_should_rerender_shadows; }
    bool GetShouldRenderShadowsForModels() { return m_should_render_shadows_for_models; }

    void SetShouldUsePickingShaderForModels(bool should_use_picking_shader_for_models) { m_should_use_picking_shader_for_models = should_use_picking_shader_for_models; }
    bool GetShouldUsePickingShaderForModels() { return m_should_use_picking_shader_for_models; }

    void SetShouldRenderShoreWaves(bool should_render_shore_waves) { m_should_render_shore_waves = should_render_shore_waves; }
    bool GetShouldRenderShoreWaves() { return m_should_render_shore_waves; }

    PerCameraCB GetPerCameraCB() { return m_per_camera_cb_data; }
    void SetPerCameraCB(const PerCameraCB& per_camera_cb_data) { m_per_camera_cb_data = per_camera_cb_data; }

    void Update(const double dt_seconds)
    {
        static double time_elapsed = 0;
        time_elapsed += dt_seconds;

        // Walk
        if (m_input_manager->IsKeyDown('W'))
            m_user_camera->Walk(WalkDirection::Forward, dt_seconds);
        if (m_input_manager->IsKeyDown('S'))
            m_user_camera->Walk(WalkDirection::Backward, dt_seconds);

        // Strafe
        if (m_input_manager->IsKeyDown('Q') || (m_input_manager->IsKeyDown('A')))
            m_user_camera->Strafe(StrafeDirection::Left, dt_seconds);
        if (m_input_manager->IsKeyDown('E') || (m_input_manager->IsKeyDown('D')))
            m_user_camera->Strafe(StrafeDirection::Right, dt_seconds);

        m_user_camera->Update(dt_seconds);

        // Center sky around camera (not height, only x and z, leave height as is)
        const auto sky_cb_opt = m_mesh_manager->GetMeshPerObjectData(m_sky_mesh_id);
        if (sky_cb_opt.has_value()) {
            auto sky_per_object_data = sky_cb_opt.value();

            DirectX::XMFLOAT4X4 sky_world_matrix;
            DirectX::XMStoreFloat4x4(&sky_world_matrix, DirectX::XMMatrixTranslation(m_user_camera->GetPosition3f().x, m_sky_height, m_user_camera->GetPosition3f().z));
            sky_per_object_data.world = sky_world_matrix;
            m_mesh_manager->UpdateMeshPerObjectData(m_sky_mesh_id, sky_per_object_data);
        }

        //const auto clouds_cb_opt = m_mesh_manager->GetMeshPerObjectData(m_clouds_mesh_id);
        //if (clouds_cb_opt.has_value()) {
        //    auto clouds_per_object_data = clouds_cb_opt.value();

        //    DirectX::XMFLOAT4X4 clouds_world_matrix;
        //    DirectX::XMStoreFloat4x4(&clouds_world_matrix, DirectX::XMMatrixTranslation(clouds_per_object_data.world._41, clouds_per_object_data.world._42, clouds_per_object_data.world._43));
        //    clouds_per_object_data.world = clouds_world_matrix;
        //    m_mesh_manager->UpdateMeshPerObjectData(m_clouds_mesh_id, clouds_per_object_data);
        //}

        const auto water_cb_opt = m_mesh_manager->GetMeshPerObjectData(m_water_mesh_id);
        if (water_cb_opt.has_value()) {
            auto water_per_object_data = water_cb_opt.value();

            DirectX::XMFLOAT4X4 water_world_matrix;
            DirectX::XMStoreFloat4x4(&water_world_matrix, DirectX::XMMatrixTranslation(water_per_object_data.world._31, m_water_level, water_per_object_data.world._33));
            water_per_object_data.world = water_world_matrix;
            m_mesh_manager->UpdateMeshPerObjectData(m_water_mesh_id, water_per_object_data);
        }

        // Update per frame CB
        PerFrameCB frameCB;
        frameCB.directionalLight = m_directionalLight;
        frameCB.time_elapsed = time_elapsed;
        frameCB.fog_color_rgb[0] = m_clear_color.x;
        frameCB.fog_color_rgb[1] = m_clear_color.y;
        frameCB.fog_color_rgb[2] = m_clear_color.z;
        frameCB.fog_start = m_fog_start;
        frameCB.fog_end = m_fog_end;
        frameCB.fog_start_y = m_fog_start_y;
        frameCB.fog_end_y = m_fog_end_y;
        frameCB.should_render_flags = 0;
        frameCB.should_render_flags = frameCB.should_render_flags | (m_should_render_shadows);
        frameCB.should_render_flags = frameCB.should_render_flags | (m_should_render_water_reflection << 1);
        frameCB.should_render_flags = frameCB.should_render_flags | (m_should_render_fog << 2);
        frameCB.should_render_flags = frameCB.should_render_flags | (m_should_render_shadows_for_models << 3);


        // Update the per frame constant buffer
        D3D11_MAPPED_SUBRESOURCE mappedResourceFrame;
        ZeroMemory(&mappedResourceFrame, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_frame_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceFrame);
        memcpy(mappedResourceFrame.pData, &frameCB, sizeof(PerFrameCB));
        m_deviceContext->Unmap(m_per_frame_cb.Get(), 0);

        // Update camera CB
        XMStoreFloat4x4(&m_per_camera_cb_data.view, XMMatrixTranspose(m_user_camera->GetView()));
        XMStoreFloat4x4(&m_per_camera_cb_data.projection, XMMatrixTranspose(m_user_camera->GetProj()));
        XMStoreFloat3(&m_per_camera_cb_data.position, m_user_camera->GetPosition());

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
        m_deviceContext->Map(m_per_camera_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        memcpy(mappedResource.pData, &m_per_camera_cb_data, sizeof(PerCameraCB));
        m_deviceContext->Unmap(m_per_camera_cb.Get(), 0);

        m_mesh_manager->Update(time_elapsed);
    }

    void Render(ID3D11RenderTargetView* render_target_view, ID3D11RenderTargetView* picking_render_target, ID3D11DepthStencilView* depth_stencil_view)
    {
        m_deviceContext->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

        // Render sky before anything else
        if (m_sky_mesh_id >= 0 && m_should_render_sky) {

            m_stencil_state_manager->SetDepthStencilState(DepthStencilStateType::Disabled);
            m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, m_sky_mesh_id);

            // Reenable depth write.
            m_stencil_state_manager->SetDepthStencilState(DepthStencilStateType::Enabled);
        }

        // Render Clouds
        if (m_clouds_mesh_id >= 0 && m_should_render_sky) {
            m_stencil_state_manager->SetDepthStencilState(DepthStencilStateType::Disabled);
            m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, m_clouds_mesh_id);
            m_stencil_state_manager->SetDepthStencilState(DepthStencilStateType::Enabled);
        }

        

        // picking_render_target can be null when writing to the offscreen buffer where picking isn't needed.
        if (picking_render_target) {
            ID3D11RenderTargetView* multipleRenderTargets[] = { render_target_view, picking_render_target };
            m_deviceContext->OMSetRenderTargets(2, multipleRenderTargets, depth_stencil_view);
        }

        if (m_terrain_mesh_id) {
            m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, m_terrain_mesh_id);
        }

        if (m_should_use_picking_shader_for_models) {
            m_deviceContext->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

            m_mesh_manager->Render(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, RenderSelectionState::OpaqueOnly, true,
                true, PixelShaderType::PickingShader, true, PixelShaderType::PickingShader);
        }
        else {
            m_mesh_manager->Render(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, RenderSelectionState::OpaqueOnly);
        }

        if (m_water_mesh_id) {
            m_deviceContext->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
            m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, m_water_mesh_id);
        }

        if (m_shore_mesh_ids.size() > 0 && m_should_render_shore_waves) {
            m_deviceContext->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
            for (auto& mesh_id : m_shore_mesh_ids) {
                const auto it = should_render_shore_mesh_id.find(mesh_id);
                if (it != should_render_shore_mesh_id.end() && it->second) {
                    m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                        m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, mesh_id);
                }
            }
        }

        if (m_should_use_picking_shader_for_models) {
            m_deviceContext->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
            m_mesh_manager->Render(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, RenderSelectionState::TransparentOnly, true,
                true, PixelShaderType::PickingShader, true, PixelShaderType::PickingShader);
        }
        else {
            if (picking_render_target) {
                ID3D11RenderTargetView* multipleRenderTargets[] = { render_target_view, picking_render_target };
                m_deviceContext->OMSetRenderTargets(2, multipleRenderTargets, depth_stencil_view);
            }

            m_mesh_manager->Render(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, RenderSelectionState::TransparentOnly);
        }
    }

    void RenderForReflection(ID3D11RenderTargetView* render_target_view, ID3D11DepthStencilView* depth_stencil_view)
    {
        m_deviceContext->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

        // Render sky before anything else
        if (m_sky_mesh_id >= 0 && m_should_render_sky) {

            m_stencil_state_manager->SetDepthStencilState(DepthStencilStateType::Disabled);
            m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, m_sky_mesh_id);

            // Reenable depth write.
            m_stencil_state_manager->SetDepthStencilState(DepthStencilStateType::Enabled);
        }

        // Render Clouds
        if (m_clouds_mesh_id >= 0 && m_should_render_sky) {
            m_stencil_state_manager->SetDepthStencilState(DepthStencilStateType::Disabled);
            m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, m_clouds_mesh_id);
            m_stencil_state_manager->SetDepthStencilState(DepthStencilStateType::Enabled);
        }

        if (m_terrain_mesh_id) {
            m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, m_terrain_mesh_id, RenderSelectionState::All, true, 
                true, PixelShaderType::TerrainReflectionTexturedWithShadows);
        }

        m_mesh_manager->Render(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
            m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, RenderSelectionState::All, true,
            true, PixelShaderType::OldModelReflection, true, PixelShaderType::NewModelReflection);
    }

    void RenderForShadowMap(ID3D11DepthStencilView* depth_stencil_view)
    {
        m_deviceContext->OMSetRenderTargets(0, nullptr, depth_stencil_view);

        if (m_terrain_mesh_id) {
            m_mesh_manager->RenderMesh(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
                m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, m_terrain_mesh_id, 
                RenderSelectionState::All, true, true, PixelShaderType::TerrainShadowMap);
        }

        m_mesh_manager->Render(m_pixel_shaders, m_blend_state_manager.get(), m_rasterizer_state_manager.get(),
            m_stencil_state_manager.get(), m_user_camera->GetPosition3f(), m_lod_quality, 
            RenderSelectionState::All, true, true, PixelShaderType::OldModelShadowMap, true, PixelShaderType::NewModelShadowMap);
    }

    void AddBox(float x, float y, float z, float size,
        CheckerboardTexture::ColorChoice color_choice1 = CheckerboardTexture::ColorChoice::White,
        CheckerboardTexture::ColorChoice color_choice2 = CheckerboardTexture::ColorChoice::Silver)
    {
        auto mesh_id = m_mesh_manager->AddBox({ size, size, size });
        extra_mesh_ids.push_back(mesh_id);

        DirectX::XMFLOAT4X4 meshWorldMatrix;
        DirectX::XMStoreFloat4x4(&meshWorldMatrix, DirectX::XMMatrixTranslation(x, y, z));
        PerObjectCB meshPerObjectData;
        meshPerObjectData.world = meshWorldMatrix;
        meshPerObjectData.num_uv_texture_pairs = 1;
        m_mesh_manager->UpdateMeshPerObjectData(mesh_id, meshPerObjectData);

        int texture_tile_size = 96;
        int texture_width = texture_tile_size * 2;
        int texture_height = texture_tile_size * 2;
        CheckerboardTexture checkerboard_texture(texture_width, texture_height, texture_tile_size, color_choice1, color_choice2);
        const auto checkered_tex_id =
            m_texture_manager->AddTexture((void*)checkerboard_texture.getData().data(), texture_width,
                texture_height, DXGI_FORMAT_R8G8B8A8_UNORM, 3214972 + (int)color_choice1 * 20 + (int)color_choice2);

        m_mesh_manager->SetTexturesForMesh(mesh_id, { m_texture_manager->GetTexture(checkered_tex_id) }, 3);
    }

    void AddTriangle3D(const Vertex3& a, const Vertex3& b, const Vertex3& c, float height,
        CheckerboardTexture::ColorChoice color_choice1 = CheckerboardTexture::ColorChoice::White,
        CheckerboardTexture::ColorChoice color_choice2 = CheckerboardTexture::ColorChoice::Silver)
    {
        auto mesh_id = m_mesh_manager->AddTriangle3D(a, b, c, height);
        extra_mesh_ids.push_back(mesh_id);

        PerObjectCB meshPerObjectData;
        meshPerObjectData.num_uv_texture_pairs = 1;
        meshPerObjectData.blend_flags[0][0] = 8;
        m_mesh_manager->UpdateMeshPerObjectData(mesh_id, meshPerObjectData);

        int texture_tile_size = 96;
        int texture_width = texture_tile_size * 2;
        int texture_height = texture_tile_size * 2;
        CheckerboardTexture checkerboard_texture(texture_width, texture_height, texture_tile_size, color_choice1, color_choice2);
        const auto checkered_tex_id =
            m_texture_manager->AddTexture((void*)checkerboard_texture.getData().data(), texture_width,
                texture_height, DXGI_FORMAT_R8G8B8A8_UNORM, 3214972 + (int)color_choice1 * 20 + (int)color_choice2);

        m_mesh_manager->SetTexturesForMesh(mesh_id, { m_texture_manager->GetTexture(checkered_tex_id) }, 3);
    }

    void AddTrapezoid3D(const Vertex3& a, const Vertex3& b, const Vertex3& c, const Vertex3& d, float height,
        CheckerboardTexture::ColorChoice color_choice1 = CheckerboardTexture::ColorChoice::White,
        CheckerboardTexture::ColorChoice color_choice2 = CheckerboardTexture::ColorChoice::Silver)
    {
        auto mesh_id = m_mesh_manager->AddTrapezoid3D(a, b, c, d, height);
        extra_mesh_ids.push_back(mesh_id);

        PerObjectCB meshPerObjectData;
        meshPerObjectData.num_uv_texture_pairs = 1;
        m_mesh_manager->UpdateMeshPerObjectData(mesh_id, meshPerObjectData);

        int texture_tile_size = 96;
        int texture_width = texture_tile_size * 2;
        int texture_height = texture_tile_size * 2;
        CheckerboardTexture checkerboard_texture(texture_width, texture_height, texture_tile_size, color_choice1, color_choice2);
        const auto checkered_tex_id =
            m_texture_manager->AddTexture((void*)checkerboard_texture.getData().data(), texture_width,
                texture_height, DXGI_FORMAT_R8G8B8A8_UNORM, 3214972 + (int)color_choice1 * 20 + (int)color_choice2);

        m_mesh_manager->SetTexturesForMesh(mesh_id, { m_texture_manager->GetTexture(checkered_tex_id) }, 3);
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

    Terrain* m_terrain = nullptr;
    PixelShaderType m_terrain_current_pixel_shader_type = PixelShaderType::TerrainRev;

    std::map<uint32_t, std::vector<int>> m_prop_mesh_ids;
    std::vector<int> extra_mesh_ids; // For stuff like spheres and boxes.

    bool m_is_terrain_mesh_set = false;
    int m_terrain_mesh_id = -1;
    int m_terrain_checkered_texture_id = -1;
    int m_terrain_texture_indices_id = -1;
    int m_terrain_shadow_map_id = -1;
    int m_terrain_texture_atlas_id = -1;

    int m_sky_mesh_id = -1;
    int m_clouds_mesh_id = -1;
    int m_water_mesh_id = -1;
    std::vector<int> m_shore_mesh_ids;

    DirectionalLight m_directionalLight;
    bool m_per_frame_cb_changed = true;

    LODQuality m_lod_quality = LODQuality::High;

    bool m_should_render_sky = true;
    float m_sky_height = 0;

    bool m_should_render_shadows = true;
    bool m_should_render_water_reflection = true;
    bool m_should_render_fog = true;
    bool m_should_render_shadows_for_models = true;

    bool m_should_render_shore_waves = true;
    std::map<int, bool> should_render_shore_mesh_id;

    bool m_should_rerender_shadows = false;

    bool m_should_use_picking_shader_for_models = false;

    XMFLOAT4 m_clear_color = { 0.662745118f, 0.662745118f, 0.662745118f, 1 };
    float m_fog_start = 100000000.0f;
    float m_fog_end = 100000000000.0f; 
    float m_fog_start_y = 0;
    float m_fog_end_y = 0;

    float m_water_level = 0;

    PerCameraCB m_per_camera_cb_data;
};
