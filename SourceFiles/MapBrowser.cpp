//
// MapBrowser.cpp
//

#include "pch.h"
#include "MapBrowser.h"
#include "draw_dat_browser.h"
#include "GuiGlobalConstants.h"
#include "draw_gui_for_open_dat_file.h"
#include "draw_dat_load_progress_bar.h"
#include "draw_picking_info.h"
#include "draw_ui.h"

extern void ExitMapBrowser() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

extern std::unordered_map<uint32_t, uint32_t> object_id_to_prop_index;
extern std::unordered_map<uint32_t, uint32_t> object_id_to_submodel_index;
extern int selected_map_file_index;

MapBrowser::MapBrowser(InputManager* input_manager) noexcept(false)
    : m_input_manager(input_manager),
    m_dat_manager_to_show_in_dat_browser(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_R8G8B8A8_UNORM);

    m_dat_managers.emplace(0, std::make_unique<DATManager>()); // Dat manager to store first dat file (more can be loaded later in comparison panel)

    m_deviceResources->RegisterDeviceNotify(this);

    last_frame_time = std::chrono::high_resolution_clock::now();
}

// Initialize the Direct3D resources required to run.
void MapBrowser::Initialize(HWND window, int width, int height)
{
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_map_renderer = std::make_unique<MapRenderer>(m_deviceResources->GetD3DDevice(),
        m_deviceResources->GetD3DDeviceContext(), m_input_manager);
    m_map_renderer->Initialize(m_deviceResources->GetScreenViewport().Width,
        m_deviceResources->GetScreenViewport().Height);

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    //m_timer.SetFixedTimeStep(true);
    //m_timer.SetTargetElapsedSeconds(1.0 / m_FPS_target);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // Modify the style to have rounded corners
    style.WindowRounding = 5.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;

    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext());
}

#pragma region Frame Update
// Executes the basic render loop.
void MapBrowser::Tick()
{
    // Check if the window is minimized
    if (m_mft_indices_to_extract.empty()) { // Only stop updates and rendering if we are not extracting stuff.
        if (IsIconic(m_deviceResources->GetWindow()))
        {
            return;  // Don't proceed with the rest of the function if the window is minimized
        }

        // Check if the window is not the foreground window
        if (GetForegroundWindow() != m_deviceResources->GetWindow())
        {
            return;  // Don't proceed with the rest of the function if the window is not in focus
        }
    }

    // Calculate the desired frame duration
    milliseconds frame_duration(1000 / m_FPS_target);

    // Get the current time
    auto now = high_resolution_clock::now();

    // Calculate the duration since the last frame
    duration<double, std::milli> elapsed = now - last_frame_time;

    // Check if the elapsed time is less than the frame duration
    if (elapsed < frame_duration) {
        return;
    }

    // Update the last frame time
    last_frame_time = high_resolution_clock::now();

    m_timer.Tick([&]() {
        Update(elapsed);
        Render();
        });
}

// Updates the world.
void MapBrowser::Update(duration<double, std::milli> elapsed)
{
    if (gw_dat_path_set && m_dat_managers[0]->m_initialization_state == InitializationState::NotStarted)
    {
        bool succeeded = m_dat_managers[0]->Init(gw_dat_path);
        if (!succeeded)
        {
            gw_dat_path_set = false;
            gw_dat_path = L"";
        }
    }

    if (m_dat_managers[0]->m_initialization_state == InitializationState::Completed && !m_hash_index_initialized) {
        const auto& mft = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT();
        for (int i = 0; i < mft.size(); i++) {
            m_hash_index[mft[i].Hash].push_back(i);
        }

        m_hash_index_initialized = true;
    }

    m_map_renderer->Update(elapsed.count() / 1000);
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void MapBrowser::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // If it is the first time rendering a map generate a shadow map
    RenderShadows();

    RenderWaterReflection();

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    m_map_renderer->Render(m_deviceResources->GetRenderTargetView(), m_deviceResources->GetPickingRenderTargetView(), m_deviceResources->GetDepthStencilView());

    // Resolve multisampled picking texture if neccessary
    if (m_deviceResources->GetMsaaLevelIndex() > 0) {
        m_deviceResources->GetD3DDeviceContext()->ResolveSubresource(
            m_deviceResources->GetPickingNonMsaaTexture(),
            0,
            m_deviceResources->GetPickingRenderTarget(),
            0,
            m_deviceResources->GetBackBufferFormat());

        // Copy picking texture to staging texture for CPU access
        m_deviceResources->GetD3DDeviceContext()->CopyResource(
            m_deviceResources->GetPickingStagingTexture(),
            m_deviceResources->GetPickingNonMsaaTexture());
    }
    else {
        m_deviceResources->GetD3DDeviceContext()->CopyResource(
            m_deviceResources->GetPickingStagingTexture(),
            m_deviceResources->GetPickingRenderTarget());
    }

    auto mouse_client_coords = m_input_manager->GetClientCoords(m_deviceResources->GetWindow());
    int hovered_object_id = m_map_renderer->GetObjectId(m_deviceResources->GetPickingStagingTexture(), mouse_client_coords.x, mouse_client_coords.y);

    // Get prop_index id
    int prop_index = -1;
    int submodel_index = -1;
    if (const auto it = object_id_to_prop_index.find(hovered_object_id); it != object_id_to_prop_index.end())
    {
        prop_index = it->second;
    }

    if (const auto it = object_id_to_submodel_index.find(hovered_object_id); it != object_id_to_submodel_index.end())
    {
        submodel_index = it->second;
    }

    PickingInfo picking_info;
    picking_info.client_x = mouse_client_coords.x;
    picking_info.client_y = mouse_client_coords.y;
    picking_info.object_id = hovered_object_id;
    picking_info.prop_index = prop_index;
    picking_info.prop_submodel_index = submodel_index;
    picking_info.camera_pos = m_map_renderer->GetCamera()->GetPosition3f();

    bool msaa_changed = false;
    int msaa_level_index = m_deviceResources->GetMsaaLevelIndex();
    const auto& msaa_levels = m_deviceResources->GetMsaaLevels();

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (m_show_error_msg) {
        ShowErrorMessage();
    }
    else {
        draw_ui(m_dat_managers, m_dat_manager_to_show_in_dat_browser, m_map_renderer.get(), picking_info, m_csv_data, m_FPS_target, m_timer, m_extract_panel_info,
            msaa_changed, msaa_level_index, msaa_levels, m_hash_index);

        if (!m_mft_indices_to_extract.empty()) {
            draw_dat_load_progress_bar(m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_num_files_for_type(FFNA_Type3) - m_mft_indices_to_extract.size(),
                m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_num_files_for_type(FFNA_Type3));

            DrawStopExtractionButton();
        }

        static bool show_demo_window = false;
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
    }

    // Dear ImGui Render
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    m_deviceResources->PIXEndEvent();

    // Show the new frame.
    m_deviceResources->Present();

    if (msaa_changed) {
        m_deviceResources->SetMsaaLevel(msaa_level_index);
    }

    if (m_extract_panel_info.pixels_per_tile_changed) {
        m_extract_panel_info.pixels_per_tile_changed = false;
        m_mft_indices_to_extract.clear();

        const auto& mft = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT();

        switch (m_extract_panel_info.map_render_extract_map_type) {
        case ExtractPanel::AllMapsTopDownOrthographic:
            for (int i = 0; i < mft.size(); i++) {
                if (mft[i].type == FFNA_Type3) {
                    m_mft_indices_to_extract.emplace(i);
                }
            }
            break;
        case ExtractPanel::CurrentMapTopDownOrthographic:
        case ExtractPanel::CurrentMapNoViewChange:
            // Add the indice for the current map
            if (selected_map_file_index >= 0) {
                m_mft_indices_to_extract.emplace(selected_map_file_index);
            }
            else {
                m_error_msg = "No map selected. Make sure to select a map before extracting or select extract all maps.";
                m_show_error_msg = true;
            }
            break;
        default:
            break;
        }
    }

    if (!m_mft_indices_to_extract.empty()) {
        const auto index = *m_mft_indices_to_extract.begin();
        m_mft_indices_to_extract.erase(index);

        bool should_save = true;

        if ((m_extract_panel_info.map_render_extract_map_type == ExtractPanel::AllMapsTopDownOrthographic ||
            m_extract_panel_info.map_render_extract_map_type == ExtractPanel::CurrentMapTopDownOrthographic)) {
            if ((m_extract_panel_info.map_render_extract_map_type == ExtractPanel::CurrentMapTopDownOrthographic ||
                parse_file(m_dat_managers[m_dat_manager_to_show_in_dat_browser].get(), index, m_map_renderer.get(), m_hash_index)
                ) && m_map_renderer->GetTerrain()) {
                const auto dim_x = m_map_renderer->GetTerrain()->m_grid_dim_x;
                const auto dim_z = m_map_renderer->GetTerrain()->m_grid_dim_z;

                const float map_width = (dim_x - 1) * 96;
                const float map_height = (dim_z - 1) * 96;

                const auto terrain_bounds = m_map_renderer->GetTerrain()->m_bounds;

                const float cam_pos_x = terrain_bounds.map_min_x + map_width / 2;
                const float cam_pos_z = terrain_bounds.map_min_z + map_height / 2;

                m_map_renderer->SetFrustumAsOrthographic(map_width - 48, map_height - 48, 100, 100000);
                m_map_renderer->GetCamera()->SetOrientation(-90.0f * XM_PI / 180, 0 * XM_PI / 180);
                m_map_renderer->GetCamera()->SetPosition(cam_pos_x, 80000, cam_pos_z - 48);

                m_map_renderer->Update(0); // Update camera
            }
            else {
                should_save = false;
            }
        }

        if (m_map_renderer->GetTerrain() == nullptr) {
            should_save = false;
            m_error_msg = "Terrain data not found.";
            m_show_error_msg = true;
        }

        if (should_save) {
            // We might disable sky rendering for top down orthographic extraction.
            // Save the current state here so we can restore it after.
            bool should_render_sky = m_map_renderer->GetShouldRenderSky();
            bool should_render_fog = m_map_renderer->GetShouldRenderFog();

            auto dim_x = m_map_renderer->GetTerrain()->m_grid_dim_x;
            auto dim_z = m_map_renderer->GetTerrain()->m_grid_dim_z;

            switch (m_extract_panel_info.map_render_extract_map_type) {
            case ExtractPanel::AllMapsTopDownOrthographic:
            case ExtractPanel::CurrentMapTopDownOrthographic:
            {
                m_deviceResources->UpdateOffscreenResources(dim_x * m_extract_panel_info.pixels_per_tile_x, dim_z * m_extract_panel_info.pixels_per_tile_y);
                m_map_renderer->SetShouldRenderSky(false);
                m_map_renderer->SetShouldRenderFog(false);

            }
            break;
            case ExtractPanel::CurrentMapNoViewChange:
            {
                int res_x = dim_x * m_extract_panel_info.pixels_per_tile_x;
                int res_y = res_x / m_map_renderer->GetCamera()->GetAspectRatio();
                m_deviceResources->UpdateOffscreenResources(res_x, res_y);
                break;
            }
            default:
                break;
            }

            RenderShadows();
            RenderWaterReflection();

            ClearOffscreen();

            m_map_renderer->Render(m_deviceResources->GetOffscreenRenderTargetView(), nullptr, m_deviceResources->GetOffscreenDepthStencilView());

            m_map_renderer->SetShouldRenderSky(should_render_sky);
            m_map_renderer->SetShouldRenderFog(should_render_fog);

            m_deviceResources->GetD3DDeviceContext()->Flush();

            ID3D11ShaderResourceView* shaderResourceView = nullptr;
            ID3D11Texture2D* texture = m_deviceResources->GetOffscreenRenderTarget();

            ID3D11Texture2D* resolvedTexture = m_deviceResources->GetOffscreenNonMsaaRenderTarget();

            m_deviceResources->GetD3DDeviceContext()->ResolveSubresource(
                resolvedTexture,  // Destination texture
                0,  // Destination subresource
                texture,  // Source texture (MSAA)
                0,  // Source subresource
                m_deviceResources->GetBackBufferFormat()  // Format
            );

            texture->Release();

            if (m_extract_panel_info.map_render_extract_file_type == ExtractPanel::PNG) {
                D3D11_TEXTURE2D_DESC textureDesc;
                resolvedTexture->GetDesc(&textureDesc);

                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
                ZeroMemory(&srvDesc, sizeof(srvDesc));
                srvDesc.Format = textureDesc.Format;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MostDetailedMip = 0;
                srvDesc.Texture2D.MipLevels = 1;

                // Create the shader resource view.
                HRESULT hr = m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
                    resolvedTexture, &srvDesc, &shaderResourceView);
                if (SUCCEEDED(hr))
                {
                    const auto file_id = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT()[index].Hash;
                    auto filename = std::format(L"{}\\map_texture_{}.png", m_extract_panel_info.save_directory, file_id);
                    SaveTextureToPng(shaderResourceView, filename, m_map_renderer->GetTextureManager());

                    shaderResourceView->Release();
                }
                else {
                    // Handle error (unable to save to png)
                }
            }
            else {
                DirectX::ScratchImage capturedImage;
                HRESULT hrCapture = DirectX::CaptureTexture(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext(), texture, capturedImage);

                if (SUCCEEDED(hrCapture)) {
                    DirectX::ScratchImage mipmappedImage;
                    HRESULT hrMip = DirectX::GenerateMipMaps(*capturedImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, mipmappedImage);
                    if (SUCCEEDED(hrMip)) {
                        DirectX::ScratchImage compressedImage;
                        HRESULT hrCompress = DirectX::Compress(mipmappedImage.GetImages(), mipmappedImage.GetImageCount(), mipmappedImage.GetMetadata(), DXGI_FORMAT_BC3_UNORM, DirectX::TEX_COMPRESS_DEFAULT, 0.5f, compressedImage);
                        if (SUCCEEDED(hrCompress)) {
                            const auto file_id = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT()[index].Hash;
                            auto filename = std::format(L"{}\\map_texture_{}.dds", m_extract_panel_info.save_directory, file_id);

                            HRESULT hrSave = DirectX::SaveToDDSFile(compressedImage.GetImages(), compressedImage.GetImageCount(), compressedImage.GetMetadata(), DDS_FLAGS_NONE, filename.c_str());
                            if (FAILED(hrSave)) {
                                m_mft_indices_to_extract.clear();

                                m_error_msg = std::format("SaveToDDSFile failed. Try lowering the resolution (i.e. use less pixels per tile).");
                                m_show_error_msg = true;
                            }
                        }
                        else {
                            m_mft_indices_to_extract.clear();

                            m_error_msg = std::format("Compress failed. Try lowering the resolution (i.e. use less pixels per tile).");
                            m_show_error_msg = true;
                        }
                    }
                    else {
                        m_mft_indices_to_extract.clear();

                        m_error_msg = std::format("GenerateMipMaps failed. Try lowering the resolution (i.e. use less pixels per tile).");
                        m_show_error_msg = true;
                    }
                }
                else {
                    m_mft_indices_to_extract.clear();

                    m_error_msg = std::format("CaptureTexture failed. Not enough VRAM, try lowering the resolution (i.e. use less pixels per tile).");
                    m_show_error_msg = true;
                }
            }

        }
    }
}

void MapBrowser::RenderWaterReflection()
{
    if (m_map_renderer->GetTerrain() && m_map_renderer->GetWaterMeshId() >= 0 && m_map_renderer->GetShouldRenderWaterReflection()) {
        bool should_render_sky = m_map_renderer->GetShouldRenderSky();
        m_map_renderer->SetShouldRenderSky(false);
        const auto camera_type = m_map_renderer->GetCamera()->GetCameraType();
        const auto camera_pos = m_map_renderer->GetCamera()->GetPosition3f();
        const auto camera_pitch = m_map_renderer->GetCamera()->GetPitch();
        const auto camera_yaw = m_map_renderer->GetCamera()->GetYaw();
        const auto camera_fovY = m_map_renderer->GetCamera()->GetFovY();
        const auto camera_frustrum_width = m_map_renderer->GetCamera()->GetViewWidth();
        const auto camera_frustrum_height = m_map_renderer->GetCamera()->GetViewHeight();
        const auto camera_aspect_ration = m_map_renderer->GetCamera()->GetAspectRatio();
        const auto camera_near = m_map_renderer->GetCamera()->GetNearZ();
        const auto camera_far = m_map_renderer->GetCamera()->GetFarZ();

        float water_level = m_map_renderer->GetWaterLevel();
        const auto cam_look = NormalizeXMFLOAT3(m_map_renderer->GetCamera()->GetLook3f());

        const XMVECTOR reflection_dir{ cam_look.x, -cam_look.y, cam_look.z };

        // Place the camera under the water surface by relecting the camera y position around the xz plane.
        m_map_renderer->GetCamera()->SetPosition(
            camera_pos.x,
            2 * water_level - camera_pos.y,
            camera_pos.z);
        m_map_renderer->GetCamera()->LookAt(reflection_dir, { 0,-1,0 });
        m_map_renderer->GetCamera()->Update(0);

#ifdef _DEBUG
        constexpr float reflection_width = 16384 / 20;
        constexpr float reflection_height = 16384 / 20;
#else
        constexpr float reflection_width = 16384 / 10;
        constexpr float reflection_height = 16384 / 10;
#endif

        // Update Camera Constant Buffer with light view projection matrix
        const auto view = m_map_renderer->GetCamera()->GetView();
        const auto proj = m_map_renderer->GetCamera()->GetProj();
        auto curr_per_camera_cb = m_map_renderer->GetPerCameraCB();
        XMStoreFloat4x4(&curr_per_camera_cb.reflection_view, XMMatrixTranspose(view));
        XMStoreFloat4x4(&curr_per_camera_cb.reflection_proj, XMMatrixTranspose(proj));
        curr_per_camera_cb.reflection_texel_size_x = 1.0f / reflection_width;
        curr_per_camera_cb.reflection_texel_size_y = 1.0f / reflection_height;
        m_map_renderer->SetPerCameraCB(curr_per_camera_cb);

        m_map_renderer->Update(0); // Update camera CB

        m_deviceResources->CreateReflectionResources(reflection_width, reflection_height);
        ClearReflection();
        m_map_renderer->RenderForReflection(m_deviceResources->GetReflectionRTV(), m_deviceResources->GetReflectionDSV());

        auto reflectionSRV = m_deviceResources->GetReflectionSRV();

        int water_mesh_id = m_map_renderer->GetWaterMeshId();
        auto water_mesh_instance = m_map_renderer->GetMeshManager()->GetMesh(water_mesh_id);

        water_mesh_instance->SetTextures({ reflectionSRV }, 2);

        // Restore settings
        m_map_renderer->GetCamera()->SetPosition(camera_pos.x, camera_pos.y, camera_pos.z);
        m_map_renderer->GetCamera()->SetOrientation(camera_pitch, camera_yaw);


        m_map_renderer->Update(0); // Update camera CB

        m_map_renderer->SetShouldRenderSky(should_render_sky);
    }
}

void MapBrowser::RenderShadows()
{
    if (m_map_renderer->GetTerrain() && m_map_renderer->GetShouldRerenderShadows()) {
        m_map_renderer->SetShouldRerenderShadows(false);

        bool should_render_sky = m_map_renderer->GetShouldRenderSky();
        m_map_renderer->SetShouldRenderSky(false);
        const auto camera_type = m_map_renderer->GetCamera()->GetCameraType();
        const auto camera_pos = m_map_renderer->GetCamera()->GetPosition3f();
        const auto camera_pitch = m_map_renderer->GetCamera()->GetPitch();
        const auto camera_yaw = m_map_renderer->GetCamera()->GetYaw();
        const auto camera_fovY = m_map_renderer->GetCamera()->GetFovY();
        const auto camera_frustrum_width = m_map_renderer->GetCamera()->GetViewWidth();
        const auto camera_frustrum_height = m_map_renderer->GetCamera()->GetViewHeight();
        const auto camera_aspect_ration = m_map_renderer->GetCamera()->GetAspectRatio();
        const auto camera_near = m_map_renderer->GetCamera()->GetNearZ();
        const auto camera_far = m_map_renderer->GetCamera()->GetFarZ();

        XMFLOAT3 corners[8] = {
            { m_map_renderer->GetTerrain()->m_bounds.map_min_x, m_map_renderer->GetTerrain()->m_bounds.map_min_y, m_map_renderer->GetTerrain()->m_bounds.map_min_z }, // Min corner
            { m_map_renderer->GetTerrain()->m_bounds.map_max_x, m_map_renderer->GetTerrain()->m_bounds.map_min_y, m_map_renderer->GetTerrain()->m_bounds.map_min_z }, // Min y, max x, min z
            { m_map_renderer->GetTerrain()->m_bounds.map_min_x, m_map_renderer->GetTerrain()->m_bounds.map_max_y, m_map_renderer->GetTerrain()->m_bounds.map_min_z }, // Max y, min x, min z
            { m_map_renderer->GetTerrain()->m_bounds.map_max_x, m_map_renderer->GetTerrain()->m_bounds.map_max_y, m_map_renderer->GetTerrain()->m_bounds.map_min_z }, // Max y, max x, min z
            { m_map_renderer->GetTerrain()->m_bounds.map_min_x, m_map_renderer->GetTerrain()->m_bounds.map_min_y, m_map_renderer->GetTerrain()->m_bounds.map_max_z }, // Min y, min x, max z
            { m_map_renderer->GetTerrain()->m_bounds.map_max_x, m_map_renderer->GetTerrain()->m_bounds.map_min_y, m_map_renderer->GetTerrain()->m_bounds.map_max_z }, // Min y, max x, max z
            { m_map_renderer->GetTerrain()->m_bounds.map_min_x, m_map_renderer->GetTerrain()->m_bounds.map_max_y, m_map_renderer->GetTerrain()->m_bounds.map_max_z }, // Max y, min x, max z
            { m_map_renderer->GetTerrain()->m_bounds.map_max_x, m_map_renderer->GetTerrain()->m_bounds.map_max_y, m_map_renderer->GetTerrain()->m_bounds.map_max_z }  // Max corner
        };


        // each of these are between -1 and 1 is the vector the light is moving
        const float cam_pos_x = -m_map_renderer->GetDirectionalLight().direction.x;
        const float cam_pos_y = -m_map_renderer->GetDirectionalLight().direction.y;
        const float cam_pos_z = -m_map_renderer->GetDirectionalLight().direction.z;
        constexpr float pos_scale = 50000;

        m_map_renderer->GetCamera()->SetPosition(cam_pos_x * pos_scale, cam_pos_y * pos_scale, cam_pos_z * pos_scale);
        m_map_renderer->GetCamera()->LookAt(m_map_renderer->GetCamera()->GetPosition(), { 0,0,0 }, { 0,1,0 });
        m_map_renderer->GetCamera()->Update(0);
        auto view = m_map_renderer->GetCamera()->GetView();

        // Use view matrix to compute new bounding box.
        XMFLOAT3 transformedCorners[8];
        for (int i = 0; i < 8; ++i) {
            XMVECTOR corner = XMLoadFloat3(&corners[i]);
            corner = XMVector3TransformCoord(corner, view);
            XMStoreFloat3(&transformedCorners[i], corner);
        }

        float minX = FLT_MAX, maxX = -FLT_MAX, minY = FLT_MAX, maxY = -FLT_MAX, minZ = FLT_MAX, maxZ = -FLT_MAX;

        for (auto& corner : transformedCorners) {
            minX = std::min(minX, corner.x);
            maxX = std::max(maxX, corner.x);
            minY = std::min(minY, corner.y);
            maxY = std::max(maxY, corner.y);
            minZ = std::min(minZ, corner.z);
            maxZ = std::max(maxZ, corner.z);
        }

        float frustum_width = maxX - minX;
        float frustum_height = maxY - minY;
        float frustum_near = minZ;
        float frustum_far = maxZ;


        m_map_renderer->SetFrustumAsOrthographic(frustum_width, frustum_height, frustum_near, frustum_far);
        m_map_renderer->GetCamera()->Update(0);

#ifdef _DEBUG
        constexpr float shadowmap_width = 16384 / 2;
        constexpr float shadowmap_height = 16384 / 2;
#else
        constexpr float shadowmap_width = 16384;
        constexpr float shadowmap_height = 16384;
#endif

        // Update Camera Constant Buffer with light view projection matrix
        view = m_map_renderer->GetCamera()->GetView();
        const auto proj = m_map_renderer->GetCamera()->GetProj();
        auto curr_per_camera_cb = m_map_renderer->GetPerCameraCB();
        XMStoreFloat4x4(&curr_per_camera_cb.directional_light_view, XMMatrixTranspose(view));
        XMStoreFloat4x4(&curr_per_camera_cb.directional_light_proj, XMMatrixTranspose(proj));
        curr_per_camera_cb.shadowmap_texel_size_x = 1.0f / shadowmap_width;
        curr_per_camera_cb.shadowmap_texel_size_y = 1.0f / shadowmap_height;
        m_map_renderer->SetPerCameraCB(curr_per_camera_cb);

        m_map_renderer->Update(0); // Update camera CB

        m_deviceResources->CreateShadowResources(shadowmap_width, shadowmap_height);
        ClearShadow();
        m_map_renderer->RenderForShadowMap(m_deviceResources->GetShadowMapDSV());

        auto shadowMapSRV = m_deviceResources->GetShadowMapSRV();

        int terrain_mesh_id = m_map_renderer->GetTerrainMeshId();
        auto terrain_mesh_instance = m_map_renderer->GetMeshManager()->GetMesh(terrain_mesh_id);
        terrain_mesh_instance->SetTextures({ shadowMapSRV }, 3);

        const auto& props_mesh_ids = m_map_renderer->GetPropsMeshIds();
        for (const auto& prop_mesh_ids : props_mesh_ids) {
            for (const auto prop_mesh_id : prop_mesh_ids.second) {
                auto prop_mesh_instance = m_map_renderer->GetMeshManager()->GetMesh(prop_mesh_id);
                prop_mesh_instance->SetTextures({ shadowMapSRV }, 0);
            }
        }

        // Restore settings
        if (camera_type == CameraType::Perspective)
        {
            m_map_renderer->GetCamera()->SetPosition(camera_pos.x, camera_pos.y, camera_pos.z);
            m_map_renderer->GetCamera()->SetFrustumAsPerspective(camera_fovY, camera_aspect_ration, camera_near, camera_far);
            m_map_renderer->GetCamera()->SetOrientation(camera_pitch, camera_yaw);

        }
        else
        {
            m_map_renderer->GetCamera()->SetPosition(camera_pos.x, camera_pos.y, camera_pos.z);
            m_map_renderer->GetCamera()->SetFrustumAsOrthographic(camera_frustrum_width, camera_frustrum_height / camera_aspect_ration, camera_near, camera_far);
            m_map_renderer->GetCamera()->SetOrientation(-90.0f * XM_PI / 180, 0 * XM_PI / 180);
        }

        m_map_renderer->Update(0); // Update camera CB

        m_map_renderer->SetShouldRenderSky(should_render_sky);
    }
}

// Helper method to clear the back buffers.
void MapBrowser::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto pickingRenderTarget = m_deviceResources->GetPickingRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    const auto& clear_color = m_map_renderer->GetClearColor();
    context->ClearRenderTargetView(renderTarget, (float*)(&clear_color));
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    ID3D11RenderTargetView* multipleRenderTargets[] = { renderTarget, pickingRenderTarget };
    context->OMSetRenderTargets(2, multipleRenderTargets, depthStencil);

    // Set the viewport.
    auto const viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}
#pragma endregion

void MapBrowser::ClearOffscreen() {
    m_deviceResources->PIXBeginEvent(L"ClearOffscreen");
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetOffscreenRenderTargetView();
    auto depthStencil = m_deviceResources->GetOffscreenDepthStencilView();

    const auto& clear_color = m_map_renderer->GetClearColor();
    context->ClearRenderTargetView(renderTarget, (float*)(&clear_color));
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    auto const viewport = m_deviceResources->GetOffscreenViewport();
    context->RSSetViewports(1, &viewport);
    m_deviceResources->PIXEndEvent();
}

void MapBrowser::ClearShadow()
{
    m_deviceResources->PIXBeginEvent(L"ClearShadow");
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto depthStencil = m_deviceResources->GetShadowMapDSV();

    context->OMSetRenderTargets(0, nullptr, depthStencil);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);


    context->OMSetRenderTargets(0, nullptr, depthStencil);

    auto const viewport = m_deviceResources->GetShadowViewport();
    context->RSSetViewports(1, &viewport);
    m_deviceResources->PIXEndEvent();
}

void MapBrowser::ClearReflection()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetReflectionRTV();
    auto depthStencil = m_deviceResources->GetReflectionDSV();

    const auto& clear_color = m_map_renderer->GetClearColor();
    context->ClearRenderTargetView(renderTarget, (float*)(&clear_color));
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto const viewport = m_deviceResources->GetReflectionViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}

void MapBrowser::ShowErrorMessage() {
    if (m_show_error_msg) {
        // Set the window to be centered
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
            ImGui::GetIO().DisplaySize.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        // Set the window to have a red border
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

        // Create a window that is not resizable, not movable and with a title bar
        ImGui::Begin("Error", &m_show_error_msg, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // Display the error message
        ImGui::Text("%s", m_error_msg.c_str());

        // Close button
        if (ImGui::Button("Close")) {
            m_show_error_msg = false;
        }

        // End the window
        ImGui::End();

        // Pop the style color for the border
        ImGui::PopStyleColor();
    }
}

#pragma region Message Handlers
// Message handlers
void MapBrowser::OnActivated()
{
    // TODO: MapBrowser is becoming active window.
}

void MapBrowser::OnDeactivated()
{
    // TODO: MapBrowser is becoming background window.
}

void MapBrowser::OnSuspending()
{
    // TODO: MapBrowser is being power-suspended (or minimized).
}

void MapBrowser::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: MapBrowser is being power-resumed (or returning from minimize).
}

void MapBrowser::OnWindowMoved()
{
    auto const r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void MapBrowser::OnDisplayChange() { m_deviceResources->UpdateColorSpace(); }

void MapBrowser::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: MapBrowser window is being resized.
    m_map_renderer->OnViewPortChanged(width, height);
}

// Properties
void MapBrowser::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 1600;
    height = 900;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void MapBrowser::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    // TODO: Initialize device dependent objects here (independent of window size).
    device;
}

// Allocate all memory resources that change on a window SizeChanged event.
void MapBrowser::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.
}

void MapBrowser::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
}

void MapBrowser::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}

void MapBrowser::DrawStopExtractionButton() {
    if (!m_mft_indices_to_extract.empty()) {
        // Set the window to be centered on the screen
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
            ImGui::GetIO().DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        // Push style color for the window border to be red
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

        // Set the border size to make the red frame visible
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

        // Begin a new window for the button
        ImGui::Begin("Stop Extraction", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

        // Render the button and handle its press
        if (ImGui::Button("Stop Extracting")) {
            m_mft_indices_to_extract.clear();
        }

        // End the window
        ImGui::End();

        // Pop the style color and style var to revert back to the default style
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
}
#pragma endregion
