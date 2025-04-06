// MapBrowser.cpp

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
using std::chrono::duration;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

extern std::unordered_map<uint32_t, uint32_t> object_id_to_prop_index;
extern std::unordered_map<uint32_t, uint32_t> object_id_to_submodel_index;
extern int selected_map_file_index;

MapBrowser::MapBrowser(InputManager* input_manager) noexcept(false)
    : m_input_manager(input_manager),
    m_dat_manager_to_show_in_dat_browser(0),
    m_total_textures_to_extract(0),
    m_hash_index_initialized(false),
    m_FPS_target(60),
    m_show_error_msg(false),
    m_is_texture_error_log_open(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_R8G8B8A8_UNORM);
    m_dat_managers.emplace(0, std::make_unique<DATManager>()); // Dat manager to store first dat file (more can be loaded later in comparison panel)
    m_deviceResources->RegisterDeviceNotify(this);
    last_frame_time = high_resolution_clock::now();
}

MapBrowser::~MapBrowser()
{
    CloseTextureErrorLog(); // Ensure log file is closed on exit
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
    m_map_renderer->Initialize(static_cast<float>(width), static_cast<float>(height));


    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    ImGui::PushStyleColor(ImGuiCol_TitleBg, style.Colors[ImGuiCol_WindowBg]);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, style.Colors[ImGuiCol_WindowBg]);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext());
}

#pragma region Frame Update
// Executes the basic render loop.
void MapBrowser::Tick()
{
    // Check if extraction is in progress; if so, don't skip frames
    bool is_extracting = !m_mft_indices_to_extract.empty() || !m_mft_indices_to_extract_textures.empty();

    if (!is_extracting) {
        if (IsIconic(m_deviceResources->GetWindow())) {
            return;
        }
        if (GetForegroundWindow() != m_deviceResources->GetWindow()) {
            return;
        }
    }


    // Calculate the desired frame duration
    milliseconds frame_duration(1000 / m_FPS_target);

    // Get the current time
    auto now = high_resolution_clock::now();

    // Calculate the duration since the last frame
    duration<double, std::milli> elapsed = now - last_frame_time;

    // Check if the elapsed time is less than the frame duration (unless extracting)
    if (elapsed < frame_duration && !is_extracting) {
        // Sleep for a short duration if not extracting to yield CPU time
        std::this_thread::sleep_for(milliseconds(1));
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

    // Check if the currently shown DAT manager is fully initialized before building hash index
    if (m_dat_managers.count(m_dat_manager_to_show_in_dat_browser) > 0 &&
        m_dat_managers[m_dat_manager_to_show_in_dat_browser]->m_initialization_state == InitializationState::Completed &&
        !m_hash_index_initialized)
    {
        const auto& mft = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT();
        m_hash_index.clear(); // Clear previous index if switching DAT
        for (int i = 0; i < mft.size(); i++) {
            // Only add entries with non-zero hash, as hash 0 can have duplicates
            if (mft[i].Hash != 0) {
                m_hash_index[mft[i].Hash].push_back(i);
            }
        }
        m_hash_index_initialized = true;
    }


    m_map_renderer->Update(elapsed.count() / 1000.0);
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

    // Render shadows if needed
    RenderShadows();

    // Render reflections if needed
    RenderWaterReflection();

    // Clear the back buffer and depth stencil
    Clear();

    // Render the main scene
    m_deviceResources->PIXBeginEvent(L"Render Scene");
    m_map_renderer->Render(m_deviceResources->GetRenderTargetView(), m_deviceResources->GetPickingRenderTargetView(), m_deviceResources->GetDepthStencilView());
    m_deviceResources->PIXEndEvent();


    // --- Process Picking ---
    // Resolve multisampled picking texture if necessary
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


    // --- Start ImGui Frame ---
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    bool msaa_changed = false;
    int msaa_level_index = m_deviceResources->GetMsaaLevelIndex();
    const auto& msaa_levels = m_deviceResources->GetMsaaLevels();

    // Draw the main UI
    if (m_show_error_msg) {
        ShowErrorMessage();
    }
    else {
        draw_ui(m_dat_managers, m_dat_manager_to_show_in_dat_browser, m_map_renderer.get(), picking_info, m_csv_data, m_FPS_target, m_timer, m_extract_panel_info,
            msaa_changed, msaa_level_index, msaa_levels, m_hash_index);

        // --- Draw extraction progress UI *inside* the ImGui frame ---
        // Check if either extraction queue is active
        bool is_map_extracting = !m_mft_indices_to_extract.empty();
        bool is_texture_extracting = !m_mft_indices_to_extract_textures.empty();
        if (is_map_extracting || is_texture_extracting) {
            int total_items = is_map_extracting ?
                m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_num_files_for_type(FFNA_Type3) :
                m_total_textures_to_extract;
            int remaining_items = is_map_extracting ?
                static_cast<int>(m_mft_indices_to_extract.size()) : // Cast size_t to int
                static_cast<int>(m_mft_indices_to_extract_textures.size()); // Cast size_t to int

            // Only draw if there are items to process
            if (total_items > 0) {
                draw_dat_load_progress_bar(total_items - remaining_items, total_items);
                DrawStopExtractionButton(); // Draw the stop button as well
            }
        }
        // --- End extraction progress UI ---

        static bool show_demo_window = false;
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
    }

    // --- Render ImGui ---
    m_deviceResources->PIXBeginEvent(L"Render ImGui");
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_deviceResources->PIXEndEvent();

    // --- Present Frame ---
    m_deviceResources->Present();

    // --- Post-Present Logic (Safe to do non-UI work here) ---
    if (msaa_changed) {
        m_deviceResources->SetMsaaLevel(msaa_level_index);
    }

    // Trigger map extraction if requested
    if (m_extract_panel_info.pixels_per_tile_changed) {
        m_extract_panel_info.pixels_per_tile_changed = false;
        m_mft_indices_to_extract.clear();
        m_mft_indices_to_extract_textures.clear();
        CloseTextureErrorLog();

        // Check if the current DAT manager is initialized
        if (m_dat_managers.count(m_dat_manager_to_show_in_dat_browser) > 0 &&
            m_dat_managers[m_dat_manager_to_show_in_dat_browser]->m_initialization_state == InitializationState::Completed) {

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
        else {
            m_error_msg = "Current DAT file is not fully loaded yet. Please wait.";
            m_show_error_msg = true;
        }
    }


    // Trigger texture extraction if requested
    if (m_extract_panel_info.extract_all_textures_requested) {
        m_extract_panel_info.extract_all_textures_requested = false;
        m_mft_indices_to_extract_textures.clear();
        m_mft_indices_to_extract.clear();
        CloseTextureErrorLog();
        OpenTextureErrorLog(m_extract_panel_info.save_directory);
        m_total_textures_to_extract = 0;

        // Check if the current DAT manager is initialized
        if (m_dat_managers.count(m_dat_manager_to_show_in_dat_browser) > 0 &&
            m_dat_managers[m_dat_manager_to_show_in_dat_browser]->m_initialization_state == InitializationState::Completed) {

            const auto& mft = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT();
            for (int i = 0; i < mft.size(); ++i) {
                if (is_type_texture(static_cast<FileType>(mft[i].type))) {
                    m_mft_indices_to_extract_textures.insert(i);
                }
            }
            m_total_textures_to_extract = static_cast<int>(m_mft_indices_to_extract_textures.size());

            if (m_total_textures_to_extract == 0) {
                m_error_msg = "No texture files found in the DAT to extract.";
                m_show_error_msg = true;
                CloseTextureErrorLog();
            }
        }
        else {
            m_error_msg = "Current DAT file is not fully loaded yet. Please wait.";
            m_show_error_msg = true;
            CloseTextureErrorLog();
        }
    }


    // Process one map extraction per frame if requested
    if (!m_mft_indices_to_extract.empty()) {
        const auto index = *m_mft_indices_to_extract.begin();
        m_mft_indices_to_extract.erase(m_mft_indices_to_extract.begin()); // Use begin() iterator

        bool should_save = true;

        if ((m_extract_panel_info.map_render_extract_map_type == ExtractPanel::AllMapsTopDownOrthographic ||
            m_extract_panel_info.map_render_extract_map_type == ExtractPanel::CurrentMapTopDownOrthographic))
        {
            // Ensure the DAT manager exists and is initialized
            if (m_dat_managers.count(m_dat_manager_to_show_in_dat_browser) > 0 &&
                m_dat_managers[m_dat_manager_to_show_in_dat_browser]->m_initialization_state == InitializationState::Completed)
            {
                if ((m_extract_panel_info.map_render_extract_map_type == ExtractPanel::CurrentMapTopDownOrthographic ||
                    parse_file(m_dat_managers[m_dat_manager_to_show_in_dat_browser].get(), index, m_map_renderer.get(), m_hash_index)) && m_map_renderer->GetTerrain())
                {
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
            else {
                should_save = false; // Don't save if DAT manager isn't ready
            }
        }


        if (m_map_renderer->GetTerrain() == nullptr) {
            should_save = false;
            if (m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT()[index].uncompressedSize > 0) {
                m_error_msg = std::format("Terrain data not found for MFT index {}.", index);
                m_show_error_msg = true;
            }
        }

        if (should_save) {
            // We might disable sky rendering for top down orthographic extraction.
            // Save the current state here so we can restore it after.
            bool should_render_sky = m_map_renderer->GetShouldRenderSky();
            bool should_render_fog = m_map_renderer->GetShouldRenderFog();
            bool should_render_shadows = m_map_renderer->GetShouldRenderShadows();
            bool should_render_model_shadows = m_map_renderer->GetShouldRenderShadowsForModels();

            auto dim_x = m_map_renderer->GetTerrain()->m_grid_dim_x;
            auto dim_z = m_map_renderer->GetTerrain()->m_grid_dim_z;

            switch (m_extract_panel_info.map_render_extract_map_type) {
            case ExtractPanel::AllMapsTopDownOrthographic:
            case ExtractPanel::CurrentMapTopDownOrthographic:
            {
                m_deviceResources->UpdateOffscreenResources(dim_x * m_extract_panel_info.pixels_per_tile_x, dim_z * m_extract_panel_info.pixels_per_tile_y, (float)dim_x / dim_z);
                m_map_renderer->SetShouldRenderSky(false);
                m_map_renderer->SetShouldRenderFog(false);
                m_map_renderer->SetShouldRenderShadows(false);
                m_map_renderer->SetShouldRenderShadowsForModels(false);
            }
            break;
            case ExtractPanel::CurrentMapNoViewChange:
            {
                int res_x = dim_x * m_extract_panel_info.pixels_per_tile_x;
                int res_y = res_x / m_map_renderer->GetCamera()->GetAspectRatio();
                m_deviceResources->UpdateOffscreenResources(res_x, res_y, (float)dim_x / dim_z);
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
            m_map_renderer->SetShouldRenderShadows(should_render_shadows);
            m_map_renderer->SetShouldRenderShadowsForModels(should_render_model_shadows);

            m_deviceResources->GetD3DDeviceContext()->Flush();

            ID3D11ShaderResourceView* shaderResourceView = nullptr;
            ID3D11Texture2D* texture = m_deviceResources->GetOffscreenRenderTarget();

            ComPtr<ID3D11Texture2D> resolvedTextureCom;
            ID3D11Texture2D* textureToSave = nullptr;

            if (m_deviceResources->GetMsaaLevelIndex() > 0) {
                resolvedTextureCom = m_deviceResources->GetOffscreenNonMsaaRenderTarget();
                m_deviceResources->GetD3DDeviceContext()->ResolveSubresource(
                    resolvedTextureCom.Get(), 0, texture, 0, m_deviceResources->GetBackBufferFormat());
                textureToSave = resolvedTextureCom.Get();
            }
            else {
                textureToSave = texture;
                textureToSave->AddRef();
            }


            if (m_extract_panel_info.map_render_extract_file_type == ExtractPanel::PNG) {
                D3D11_TEXTURE2D_DESC textureDesc;
                textureToSave->GetDesc(&textureDesc);

                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                ZeroMemory(&srvDesc, sizeof(srvDesc));
                srvDesc.Format = textureDesc.Format;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MostDetailedMip = 0;
                srvDesc.Texture2D.MipLevels = 1;

                // Create the shader resource view.
                HRESULT hr = m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
                    textureToSave, &srvDesc, &shaderResourceView);
                if (SUCCEEDED(hr))
                {
                    const auto file_id = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT()[index].Hash;
                    auto filename = std::format(L"{}\\map_texture_0x{:X}.png", m_extract_panel_info.save_directory, file_id);
                    std::wstring wsFilename = filename;
                    SaveTextureToPng(shaderResourceView, wsFilename, m_map_renderer->GetTextureManager());

                    shaderResourceView->Release();
                }
                else {
                    // Handle error (unable to save to png)
                    m_error_msg = std::format("Failed to create SRV for map extraction (PNG). HRESULT: 0x{:X}", hr);
                    m_show_error_msg = true;
                    m_mft_indices_to_extract.clear();
                }
            }
            else { // DDS
                DirectX::ScratchImage capturedImage;
                HRESULT hrCapture = DirectX::CaptureTexture(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext(), textureToSave, capturedImage);

                if (SUCCEEDED(hrCapture)) {
                    DirectX::ScratchImage mipmappedImage;
                    HRESULT hrMip = DirectX::GenerateMipMaps(*capturedImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, mipmappedImage);
                    if (SUCCEEDED(hrMip)) {
                        DirectX::ScratchImage compressedImage;
                        HRESULT hrCompress = DirectX::Compress(mipmappedImage.GetImages(), mipmappedImage.GetImageCount(), mipmappedImage.GetMetadata(), DXGI_FORMAT_BC3_UNORM, DirectX::TEX_COMPRESS_DEFAULT, 0.5f, compressedImage);
                        if (SUCCEEDED(hrCompress)) {
                            const auto file_id = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT()[index].Hash;
                            auto filename = std::format(L"{}\\map_texture_0x{:X}.dds", m_extract_panel_info.save_directory, file_id);

                            HRESULT hrSave = DirectX::SaveToDDSFile(compressedImage.GetImages(), compressedImage.GetImageCount(), compressedImage.GetMetadata(), DirectX::DDS_FLAGS_NONE, filename.c_str());
                            if (FAILED(hrSave)) {
                                m_mft_indices_to_extract.clear();
                                m_error_msg = std::format("SaveToDDSFile failed. HRESULT: 0x{:X}. Try lowering the resolution (i.e. use less pixels per tile).", hrSave);
                                m_show_error_msg = true;
                            }
                        }
                        else {
                            m_mft_indices_to_extract.clear();
                            m_error_msg = std::format("Compress failed. HRESULT: 0x{:X}. Try lowering the resolution (i.e. use less pixels per tile).", hrCompress);
                            m_show_error_msg = true;
                        }
                    }
                    else {
                        m_mft_indices_to_extract.clear();
                        m_error_msg = std::format("GenerateMipMaps failed. HRESULT: 0x{:X}. Try lowering the resolution (i.e. use less pixels per tile).", hrMip);
                        m_show_error_msg = true;
                    }
                }
                else {
                    m_mft_indices_to_extract.clear();
                    m_error_msg = std::format("CaptureTexture failed. HRESULT: 0x{:X}. Not enough VRAM, try lowering the resolution (i.e. use less pixels per tile).", hrCapture);
                    m_show_error_msg = true;
                }
            }

            // Release resolved texture if it was used and we added a ref
            if (textureToSave && m_deviceResources->GetMsaaLevelIndex() == 0) {
                textureToSave->Release();
            }
        }
    }
    // Process one texture extraction per frame if requested
    if (!m_mft_indices_to_extract_textures.empty()) {
        int index_to_process = *m_mft_indices_to_extract_textures.begin();
        ProcessTextureExtraction(index_to_process, m_extract_panel_info.save_directory);
        m_mft_indices_to_extract_textures.erase(index_to_process);


        if (m_mft_indices_to_extract_textures.empty()) {
            OutputDebugStringA("Texture extraction complete.\n");
            CloseTextureErrorLog();
        }
    }
}

void MapBrowser::RenderWaterReflection()
{
    if (m_map_renderer->GetTerrain() && m_map_renderer->GetWaterMeshId() >= 0 && m_map_renderer->GetShouldRenderWaterReflection()) {
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

        m_deviceResources->CreateReflectionResources(static_cast<UINT>(reflection_width), static_cast<UINT>(reflection_height)); // Cast to UINT
        ClearReflection();
        m_map_renderer->RenderForReflection(m_deviceResources->GetReflectionRTV(), m_deviceResources->GetReflectionDSV());

        auto reflectionSRV = m_deviceResources->GetReflectionSRV();

        int water_mesh_id = m_map_renderer->GetWaterMeshId();
        auto water_mesh_instance = m_map_renderer->GetMeshManager()->GetMesh(water_mesh_id);

        water_mesh_instance->SetTextures({ reflectionSRV }, 2);

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
            m_map_renderer->GetCamera()->SetFrustumAsOrthographic(camera_frustrum_width, camera_frustrum_height, camera_near, camera_far);
            m_map_renderer->GetCamera()->SetOrientation(camera_pitch, camera_yaw);
        }

        m_map_renderer->Update(0); // Update camera CB
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

        m_deviceResources->CreateShadowResources(static_cast<UINT>(shadowmap_width), static_cast<UINT>(shadowmap_height));
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

        //const auto& shore_mesh_ids = m_map_renderer->GetShoreMeshIds();
        //for (const auto shore_mesh_id : shore_mesh_ids) {
        //    auto shore_mesh_instance = m_map_renderer->GetMeshManager()->GetMesh(shore_mesh_id);
        //    shore_mesh_instance->SetTextures({ shadowMapSRV }, 0);
        //}

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
            m_map_renderer->GetCamera()->SetFrustumAsOrthographic(camera_frustrum_width, camera_frustrum_height, camera_near, camera_far);
            m_map_renderer->GetCamera()->SetOrientation(camera_pitch, camera_yaw);
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
    // Clear picking target with a color that represents "no object" (e.g., black or white with alpha 1)
    float pickingClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Or {1,1,1,1}
    context->ClearRenderTargetView(pickingRenderTarget, pickingClearColor);
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



    auto const viewport = m_deviceResources->GetShadowViewport();
    context->RSSetViewports(1, &viewport);
    m_deviceResources->PIXEndEvent();
}

void MapBrowser::ClearReflection()
{
    m_deviceResources->PIXBeginEvent(L"ClearReflection");

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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f); // Set border size


        // Create a window that is not resizable, not movable and with a title bar
        ImGui::Begin("Error", &m_show_error_msg, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

        // Display the error message with TextWrapped
        ImGui::TextWrapped("%s", m_error_msg.c_str());

        // Close button
        if (ImGui::Button("Close")) {
            m_show_error_msg = false;
            m_error_msg = ""; // Clear error message after closing
        }

        // End the window
        ImGui::End();

        // Pop the style color and style var for the border
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
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

    m_map_renderer->OnViewPortChanged(static_cast<float>(width), static_cast<float>(height));
}

// Properties
void MapBrowser::GetDefaultSize(int& width, int& height) const noexcept
{
    width = 1600;
    height = 900;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void MapBrowser::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
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
    bool is_extracting = !m_mft_indices_to_extract.empty() || !m_mft_indices_to_extract_textures.empty();
    if (is_extracting) {
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

        // Determine button label based on which extraction is running
        const char* buttonLabel = "Stop Extraction"; // Generic default
        if (!m_mft_indices_to_extract_textures.empty()) {
            buttonLabel = "Stop Texture Extraction";
        }
        else if (!m_mft_indices_to_extract.empty()) {
            buttonLabel = "Stop Map Extraction";
        }

        // Render the button and handle its press
        if (ImGui::Button(buttonLabel)) {
            m_mft_indices_to_extract.clear(); // Clear map extraction queue
            m_mft_indices_to_extract_textures.clear(); // Clear texture extraction queue
            m_total_textures_to_extract = 0; // Reset texture count
            CloseTextureErrorLog(); // Close log file if stopped manually
        }

        // End the window
        ImGui::End();

        // Pop the style color and style var to revert back to the default style
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
}


void MapBrowser::ProcessTextureExtraction(int index, const std::wstring& save_directory)
{
    DATManager* dat_manager = m_dat_managers[m_dat_manager_to_show_in_dat_browser].get();
    const auto& mft = dat_manager->get_MFT();
    if (index < 0 || index >= mft.size()) {
        std::wstring error_str = std::format(L"Invalid MFT index {} provided for texture extraction.", index);
        OutputDebugStringW(error_str.c_str());
        WriteToTextureErrorLog(index, -1, error_str);
        return; // Continue to next item without stopping the whole process
    }

    const auto& entry = mft[index];
    unsigned char* raw_data = nullptr;
    ComPtr<ID3D11Texture2D> localTexture;
    ComPtr<ID3D11ShaderResourceView> localSrv;
    int texWidth = 0, texHeight = 0;
    std::vector<RGBA> rgba_data; // Needed for DDS parsing result
    DirectX::ScratchImage ddsImage; // To hold data from DDS

    try {
        // Always read raw file data for extraction
        raw_data = dat_manager->read_file(index);
        if (!raw_data) {
            throw std::runtime_error(std::format("Failed to read file data for index {}.", index));
        }

        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        const void* pixelDataPtr = nullptr;
        UINT bytesPerPixel = 0;
        ComPtr<ID3D11Device> device = m_deviceResources->GetD3DDevice();

        if (entry.type == DDS) {
            DirectX::TexMetadata metadata;
            HRESULT hr = DirectX::LoadFromDDSMemory(raw_data, entry.uncompressedSize, DirectX::DDS_FLAGS_NONE, &metadata, ddsImage);
            if (FAILED(hr)) {
                throw std::runtime_error(std::format("LoadFromDDSMemory failed for index {}. HRESULT: 0x{:X}", index, hr));
            }

            DXGI_FORMAT targetFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
            if (metadata.format != targetFormat)
            {
                DirectX::ScratchImage convertedImage;
                hr = DirectX::Convert(*ddsImage.GetImage(0, 0, 0), targetFormat, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, convertedImage);
                if (FAILED(hr)) {
                    throw std::runtime_error(std::format("DDS Convert failed for index {}. HRESULT: 0x{:X}", index, hr));
                }
                // ddsImage is already a ScratchImage, no need to Release, just move assign
                ddsImage = std::move(convertedImage); // Replace original with converted
                metadata = ddsImage.GetMetadata(); // Get updated metadata
            }

            format = metadata.format;
            texWidth = static_cast<int>(metadata.width);
            texHeight = static_cast<int>(metadata.height);
            pixelDataPtr = ddsImage.GetPixels();
            bytesPerPixel = BytesPerPixel(format);

        }
        else if (is_type_texture(static_cast<FileType>(entry.type))) {
            DatTexture dat_texture = dat_manager->parse_ffna_texture_file(index);
            if (dat_texture.width > 0 && dat_texture.height > 0) {
                texWidth = dat_texture.width;
                texHeight = dat_texture.height;
                format = DXGI_FORMAT_B8G8R8A8_UNORM;
                rgba_data = std::move(dat_texture.rgba_data);
                pixelDataPtr = rgba_data.data();
                bytesPerPixel = sizeof(RGBA);
            }
            else {
                throw std::runtime_error(std::format("ProcessImageFile failed or returned invalid dimensions for index {}.", index));
            }
        }
        else {
            throw std::runtime_error(std::format("Attempting to extract non-texture file at index {} as texture.", index));
        }

        if (!pixelDataPtr || texWidth <= 0 || texHeight <= 0 || bytesPerPixel == 0) {
            throw std::runtime_error(std::format("Invalid texture data derived for index {}.", index));
        }


        // Create temporary D3D11Texture2D
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = texWidth;
        texDesc.Height = texHeight;
        texDesc.MipLevels = 1; // Create only one mip level for saving
        texDesc.ArraySize = 1;
        texDesc.Format = format; // Use the determined format
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = pixelDataPtr;
        initData.SysMemPitch = texWidth * bytesPerPixel;
        initData.SysMemSlicePitch = initData.SysMemPitch * texHeight;

        HRESULT hr = device->CreateTexture2D(&texDesc, &initData, localTexture.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error(std::format("CreateTexture2D failed for index {}. HRESULT: 0x{:X}", index, hr));

        // Create temporary ID3D11ShaderResourceView
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        hr = device->CreateShaderResourceView(localTexture.Get(), &srvDesc, localSrv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error(std::format("CreateShaderResourceView failed for index {}. HRESULT: 0x{:X}", index, hr));

        // --- Create Subfolder ---
        FileType fileTypeEnum = static_cast<FileType>(entry.type);
        std::wstring typeSubfolderName = typeToWString(fileTypeEnum);
        if (typeSubfolderName == L" ") {
            typeSubfolderName = L"UnknownType";
        }
        std::filesystem::path subFolderPath = std::filesystem::path(save_directory) / typeSubfolderName;

        // Create directory if it doesn't exist
        std::error_code ec;
        if (!std::filesystem::exists(subFolderPath)) {
            if (!std::filesystem::create_directories(subFolderPath, ec)) {
                // Handle error - couldn't create directory
                throw std::runtime_error(std::format("Failed to create directory: {}. Error code: {}", subFolderPath.string(), ec.value()));
            }
        }
        // --- End Create Subfolder ---


        // Construct filename and save
        std::wstring filename = std::format(L"texture_0x{:X}.png", entry.Hash);
        std::filesystem::path fullPath = subFolderPath / filename;

        std::wstring fullPathStr = fullPath.wstring();
        if (!m_map_renderer || !m_map_renderer->GetTextureManager()) {
            throw std::runtime_error("TextureManager is not available for saving.");
        }
        if (!SaveTextureToPng(localSrv.Get(), fullPathStr, m_map_renderer->GetTextureManager())) {
            throw std::runtime_error(std::format("Failed to save texture to PNG for index {}.", index));
        }

        // Cleanup raw data if allocated
        if (raw_data) {
            delete[] raw_data;
            raw_data = nullptr;
        }
        // ComPtrs handle D3D resource release automatically
    }
    catch (const std::exception& e) {
        // Log the error instead of showing a popup and stopping
        std::string error_what = e.what();
        std::wstring error_msg_w(error_what.begin(), error_what.end());
        std::wstring log_entry = std::format(L"MFT Index: {}, File Hash: 0x{:X}, Error: {}", index, entry.Hash, error_msg_w);
        OutputDebugStringW(log_entry.c_str());
        OutputDebugStringA("\n"); // For VS Output Window clarity
        WriteToTextureErrorLog(index, entry.Hash, error_msg_w);
        if (raw_data) { delete[] raw_data; } // Ensure cleanup even on exception
    }
    catch (...) {
        // Log the unknown error
        std::wstring log_entry = std::format(L"MFT Index: {}, File Hash: 0x{:X}, Error: Unknown error.", index, entry.Hash);
        OutputDebugStringW(log_entry.c_str());
        OutputDebugStringA("\n"); // For VS Output Window clarity
        WriteToTextureErrorLog(index, entry.Hash, L"Unknown error.");
        if (raw_data) { delete[] raw_data; } // Ensure cleanup even on exception
    }
}


void MapBrowser::OpenTextureErrorLog(const std::wstring& save_directory) {
    if (m_is_texture_error_log_open) {
        m_texture_error_log_file.close();
    }
    std::filesystem::path logPath = std::filesystem::path(save_directory) / L"texture_extraction_errors.log";
    m_texture_error_log_file.open(logPath, std::ios::out | std::ios::trunc); // Open in truncate mode
    if (m_texture_error_log_file.is_open()) {
        m_is_texture_error_log_open = true;
        m_texture_error_log_file << L"Texture Extraction Errors Log\n";
        m_texture_error_log_file << L"-----------------------------\n";
        m_texture_error_log_file.flush();
    }
    else {
        OutputDebugStringA("Failed to open texture error log file.\n");
    }
}

void MapBrowser::CloseTextureErrorLog() {
    if (m_is_texture_error_log_open) {
        m_texture_error_log_file.close();
        m_is_texture_error_log_open = false;
    }
}

void MapBrowser::WriteToTextureErrorLog(int mft_index, int file_hash, const std::wstring& error_message) {
    if (m_is_texture_error_log_open && m_texture_error_log_file.is_open()) {
        m_texture_error_log_file << L"MFT Index: " << mft_index;
        if (file_hash >= 0) {
            m_texture_error_log_file << L", File Hash: 0x" << std::hex << file_hash << std::dec;
        }
        m_texture_error_log_file << L", Error: " << error_message << L"\n";
        m_texture_error_log_file.flush(); // Ensure it's written immediately
    }
}


#pragma endregion