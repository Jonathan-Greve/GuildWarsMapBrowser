//
// MapBrowser.cpp
//

#include "pch.h"
#include "MapBrowser.h"
#include "draw_dat_browser.h"
#include "GuiGlobalConstants.h"

std::wstring gw_dat_path = L"";
bool gw_dat_path_set = false;

void DrawGuiForOpenDatFile()
{
    constexpr auto window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;
    constexpr auto window_name = "Select your Gw.dat file";
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin(window_name, nullptr, window_flags);

    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 screen_size = ImGui::GetIO().DisplaySize;
    ImVec2 window_pos = (screen_size - window_size) * 0.5f;

    ImGui::SetWindowPos(window_pos);

    // Get the size of the button
    ImVec2 button_size = ImVec2(200, 40);

    // Calculate the position of the button
    float x = (window_size.x - button_size.x) / 2.0f;
    float y = (window_size.y - button_size.y) / 2.0f;

    // Set the position of the button
    ImGui::SetCursorPos(ImVec2(x, y));

    // Set the background color for a button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Select a \"Gw.dat\" File", button_size))
    {
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".dat", ".");
    }
    // Restore the original style
    ImGui::PopStyleColor(3);

    // Display File Dialog
    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", 0, ImVec2(500, 400)))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

            std::wstring wstr(filePathName.begin(), filePathName.end());
            gw_dat_path = wstr;
            gw_dat_path_set = true;
        }

        // close
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::End();
}

void drawUI()
{
    constexpr auto window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    // set up the left panel
    ImGui::SetNextWindowPos(ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::left_panel_width,
                                    ImGui::GetIO().DisplaySize.y - 2 * GuiGlobalConstants::panel_padding));
    ImGui::PushStyleVar(
      ImGuiStyleVar_WindowPadding,
      ImVec2(GuiGlobalConstants::panel_padding, GuiGlobalConstants::panel_padding)); // add padding
    ImGui::Begin("Left Panel", NULL, window_flags);

    // draw the contents of the left panel
    ImGui::Text("This is the left panel");

    ImGui::End();

    // set up the right panel
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - GuiGlobalConstants::left_panel_width -
                                     GuiGlobalConstants::panel_padding,
                                   GuiGlobalConstants::panel_padding));
    ImGui::SetNextWindowSize(ImVec2(GuiGlobalConstants::right_panel_width,
                                    ImGui::GetIO().DisplaySize.y - 2 * GuiGlobalConstants::panel_padding));
    ImGui::Begin("Right Panel", NULL, window_flags);

    // draw the contents of the right panel
    ImGui::Text("This is the right panel");

    ImGui::End();
    ImGui::PopStyleVar();
}

void draw_dat_load_progress_bar(int num_files_read, int total_num_files)
{
    ImVec2 progress_bar_window_size =
      ImVec2(ImGui::GetIO().DisplaySize.x -
               (GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2) -
               (GuiGlobalConstants::right_panel_width + GuiGlobalConstants::panel_padding * 2),
             30);
    ImVec2 progress_bar_window_pos = ImVec2(
      GuiGlobalConstants::left_panel_width + GuiGlobalConstants::panel_padding * 2,
      ImGui::GetIO().DisplaySize.y - progress_bar_window_size.y - GuiGlobalConstants::panel_padding - 2);
    ImGui::SetNextWindowPos(progress_bar_window_pos);
    ImGui::SetNextWindowSize(progress_bar_window_size);
    ImGui::Begin("Progress Bar", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    float progress = static_cast<float>(num_files_read) / static_cast<float>(total_num_files);
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x * progress, ImGui::GetContentRegionAvail().y);
    ImVec2 pos = ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 clip_min = draw_list->GetClipRectMin();
    ImVec2 clip_max = draw_list->GetClipRectMax();
    draw_list->PushClipRect(clip_min, ImVec2(clip_min.x + size.x, clip_max.y), true);
    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImColor(255, 255, 255, 255));
    draw_list->PopClipRect();

    ImGui::SetCursorPosX(progress_bar_window_size.x / 2);
    const auto progress_str =
      std::format("{:.1f}%% ({}/{})", progress * 100, num_files_read, total_num_files);
    // Create a yellow color
    ImVec4 yellowColor = ImVec4(0.9f, 0.4f, 0.0f, 1.0f); // RGBA values: (1.0, 1.0, 0.0, 1.0)

    // Set the text color to yellow
    ImGui::PushStyleColor(ImGuiCol_Text, yellowColor);
    ImGui::Text(progress_str.c_str());
    ImGui::PopStyleColor();

    ImGui::End();
}

extern void ExitMapBrowser() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

MapBrowser::MapBrowser(InputManager* input_manager) noexcept(false)
    : m_input_manager(input_manager)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    // TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
    //   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
    //   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
    m_deviceResources->RegisterDeviceNotify(this);
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
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

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
    m_timer.Tick([&]() { Update(m_timer); });

    Render();
}

// Updates the world.
void MapBrowser::Update(DX::StepTimer const& timer)
{
    if (gw_dat_path_set && m_dat_manager.m_initialization_state == InitializationState::NotStarted)
    {
        m_dat_manager.Init(gw_dat_path);
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

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (! gw_dat_path_set)
    {
        DrawGuiForOpenDatFile();
    }
    else
    {
        drawUI();
        if (m_dat_manager.m_initialization_state == InitializationState::Started)
        {
            draw_dat_load_progress_bar(m_dat_manager.get_num_files_type_read(),
                                       m_dat_manager.get_num_files());
        }
        if (m_dat_manager.m_initialization_state == InitializationState::Completed)
        {
            draw_data_browser(m_dat_manager, m_map_renderer.get());
        }
    }

    static bool show_demo_window = false;
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    // Dear ImGui Render
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    m_deviceResources->PIXEndEvent();

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void MapBrowser::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto const viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}
#pragma endregion

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
    if (! m_deviceResources->WindowSizeChanged(width, height))
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
