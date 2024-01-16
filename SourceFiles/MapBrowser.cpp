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

    using namespace std::chrono;

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
        Update(m_timer);
        Render();
        });
}

// Updates the world.
void MapBrowser::Update(DX::StepTimer const& timer)
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

    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your map browser logic here.
    m_map_renderer->Update(elapsedTime);
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

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");

    m_map_renderer->Render();

    // Copy picking texture to staging texture for CPU access
    m_deviceResources->GetD3DDeviceContext()->CopyResource(
        m_deviceResources->GetPickingStagingTexture(),
        m_deviceResources->GetPickingRenderTarget());

    auto mouse_client_coords = m_input_manager->GetClientCoords(m_deviceResources->GetWindow());
    int hovered_object_id = m_map_renderer->GetObjectId(m_deviceResources->GetPickingStagingTexture(), mouse_client_coords.x, mouse_client_coords.y);

    // Get prop_index id
    int prop_index = -1;
    if (const auto it = object_id_to_prop_index.find(hovered_object_id); it != object_id_to_prop_index.end())
    {
        prop_index = it->second;
    }

    PickingInfo picking_info;
    picking_info.client_x = mouse_client_coords.x;
    picking_info.client_y = mouse_client_coords.y;
    picking_info.object_id = hovered_object_id;
    picking_info.prop_index = prop_index;
    picking_info.camera_pos = m_map_renderer->GetCamera()->GetPosition3f();

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (m_show_error_msg) {
        ShowErrorMessage();
    }
    else {

        draw_ui(m_dat_managers, m_dat_manager_to_show_in_dat_browser, m_map_renderer.get(), picking_info, m_csv_data, m_FPS_target, m_timer, m_extract_panel_info);

        if (!m_mft_indices_to_extract.empty()) {
            draw_dat_load_progress_bar(m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_num_files_for_type(FFNA_Type3) - m_mft_indices_to_extract.size(),
                m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_num_files_for_type(FFNA_Type3));
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

    if (m_extract_panel_info.pixels_per_tile_changed) {
        m_extract_panel_info.pixels_per_tile_changed = false;
        m_mft_indices_to_extract.clear();

        const auto& mft = m_dat_managers[m_dat_manager_to_show_in_dat_browser]->get_MFT();

        for (int i = 0; i < mft.size(); i++) {
            // We only support extracting map files for now.
            if (mft[i].type == FFNA_Type3) {
                m_mft_indices_to_extract.emplace(i);
            }

            if (!m_hash_index_initialized) {
                m_hash_index[mft[i].Hash].push_back(i);
            }
        }

        m_hash_index_initialized = true;
    }

    if (!m_mft_indices_to_extract.empty()) {
        const auto index = *m_mft_indices_to_extract.begin();
        m_mft_indices_to_extract.erase(index);

        if (parse_file(m_dat_managers[m_dat_manager_to_show_in_dat_browser].get(), index, m_map_renderer.get(), m_hash_index)) {

            const auto dim_x = m_map_renderer->GetTerrain()->m_grid_dim_x;
            const auto dim_z = m_map_renderer->GetTerrain()->m_grid_dim_z;

            const float map_width = (dim_x - 1) * 96;
            const float map_height = (dim_z - 1) * 96;

            const auto terrain_bounds = m_map_renderer->GetTerrain()->m_bounds;

            const float cam_pos_x = terrain_bounds.map_min_x + map_width / 2;
            const float cam_pos_z = terrain_bounds.map_min_z + map_height / 2;

            m_map_renderer->SetFrustumAsOrthographic(map_width, map_height, 100, 100000);
            m_map_renderer->GetCamera()->SetOrientation(-90.0f * XM_PI / 180, 0 * XM_PI / 180);
            m_map_renderer->GetCamera()->SetPosition(cam_pos_x, 80000, cam_pos_z - 48);

            m_map_renderer->Update(0); // Update camera

            m_deviceResources->UpdateOffscreenResources(dim_x * m_extract_panel_info.pixels_per_tile_x, dim_z * m_extract_panel_info.pixels_per_tile_y);

            ClearOffscreen();

            m_map_renderer->Render();

            m_deviceResources->GetD3DDeviceContext()->Flush();

            ID3D11ShaderResourceView* shaderResourceView = nullptr;
            ID3D11Texture2D* texture = m_deviceResources->GetOffscreenRenderTarget();

            if (m_extract_panel_info.map_render_extract_file_type == ExtractPanel::PNG) {
                D3D11_TEXTURE2D_DESC textureDesc;
                texture->GetDesc(&textureDesc);

                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
                ZeroMemory(&srvDesc, sizeof(srvDesc));
                srvDesc.Format = textureDesc.Format;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MostDetailedMip = 0;
                srvDesc.Texture2D.MipLevels = 1;

                // Create the shader resource view.
                HRESULT hr = m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
                    texture, &srvDesc, &shaderResourceView);
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

// Helper method to clear the back buffers.
void MapBrowser::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto pickingRenderTarget = m_deviceResources->GetPickingRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
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

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    auto const viewport = m_deviceResources->GetOffscreenViewport();
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
#pragma endregion
