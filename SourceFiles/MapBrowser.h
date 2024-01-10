//
// MapBrowser.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "InputManager.h"
#include "Camera.h"
#include "DATManager.h"
#include "MapRenderer.h"

// A basic MapBrowser implementation that creates a D3D11 device and
// provides a MapBrowser loop.
class MapBrowser final : public DX::IDeviceNotify
{
public:
    MapBrowser(InputManager* input_manager) noexcept(false);
    ~MapBrowser() = default;

    MapBrowser(MapBrowser&&) = default;
    MapBrowser& operator=(MapBrowser&&) = default;

    MapBrowser(MapBrowser const&) = delete;
    MapBrowser& operator=(MapBrowser const&) = delete;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic MapBrowser loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize(int& width, int& height) const noexcept;

private:
    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources> m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer m_timer;

    int m_FPS_target = 60;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_frame_time;

    // Used for resizing the offscreen buffer when extracting arbitrary sized render of map to png or dds (Extract Panel GUI)
    int m_pixels_per_tile_x = 1;
    int m_pixels_per_tile_y = 1;
    bool m_pixels_per_tile_changed = false;

    // Input manager
    InputManager* m_input_manager;

    std::map<int, std::unique_ptr<DATManager>> m_dat_managers;
    int m_dat_manager_to_show_in_dat_browser;

    std::vector<std::vector<std::string>> m_csv_data;

    std::unique_ptr<MapRenderer> m_map_renderer;
};
