//
// MapBrowser.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

// A basic MapBrowser implementation that creates a D3D11 device and
// provides a MapBrowser loop.
class MapBrowser final : public DX::IDeviceNotify
{
public:
    MapBrowser() noexcept(false);
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
};
