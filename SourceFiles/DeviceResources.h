//
// DeviceResources.h - A wrapper for the Direct3D 11 device and swapchain
//

#pragma once

namespace DX
{
    // Provides an interface for an application that owns DeviceResources to be notified of the device being lost or created.
    interface IDeviceNotify
    {
        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;

    protected:
        ~IDeviceNotify() = default;
    };

    // Controls all the DirectX device resources.
    class DeviceResources
    {
    public:
        static constexpr unsigned int c_FlipPresent  = 0x1;
        static constexpr unsigned int c_AllowTearing = 0x2;
        static constexpr unsigned int c_EnableHDR    = 0x4;

        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
                        UINT backBufferCount = 2,
                        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_10_0,
                        unsigned int flags = c_FlipPresent) noexcept;
        ~DeviceResources() = default;

        DeviceResources(DeviceResources&&) = default;
        DeviceResources& operator= (DeviceResources&&) = default;

        DeviceResources(DeviceResources const&) = delete;
        DeviceResources& operator= (DeviceResources const&) = delete;

        void CreateDeviceResources();
        void UpdateOffscreenResources(int width, int height);
        void CreateWindowSizeDependentResources();
        void SetWindow(HWND window, int width, int height) noexcept;
        bool WindowSizeChanged(int width, int height);
        void HandleDeviceLost();
        void RegisterDeviceNotify(IDeviceNotify* deviceNotify) noexcept { m_deviceNotify = deviceNotify; }
        void Present();
        void UpdateColorSpace();

        // Device Accessors.
        RECT GetOutputSize() const noexcept { return m_outputSize; }

        // Direct3D Accessors.
        auto                    GetD3DDevice() const noexcept           { return m_d3dDevice.Get(); }
        auto                    GetD3DDeviceContext() const noexcept    { return m_d3dContext.Get(); }
        auto                    GetSwapChain() const noexcept           { return m_swapChain.Get(); }
        auto                    GetDXGIFactory() const noexcept         { return m_dxgiFactory.Get(); }
        HWND                    GetWindow() const noexcept              { return m_window; }
        D3D_FEATURE_LEVEL       GetDeviceFeatureLevel() const noexcept  { return m_d3dFeatureLevel; }
        ID3D11Texture2D*        GetRenderTarget() const noexcept        { return m_renderTarget.Get(); }
        ID3D11Texture2D*        GetPickingRenderTarget() const noexcept        { return m_pickingRenderTarget.Get(); }
        ID3D11Texture2D*        GetPickingStagingTexture() const noexcept { return m_pickingStagingTexture.Get(); }
        ID3D11Texture2D*        GetPickingNonMsaaTexture() const noexcept { return m_pickingNonMsaaTexture.Get(); }
        ID3D11Texture2D*        GetOffscreenRenderTarget() const noexcept { return m_offscreenRenderTarget.Get(); }
        ID3D11Texture2D*        GetOffscreenNonMsaaRenderTarget() const noexcept { return m_offscreenNonMsaaTexture.Get(); }
        ID3D11Texture2D*        GetDepthStencil() const noexcept { return m_depthStencil.Get(); }
        ID3D11Texture2D*        GetOffscreenDepthStencil() const noexcept { return m_offscreenDepthStencil.Get(); }
        ID3D11RenderTargetView*	GetRenderTargetView() const noexcept    { return m_d3dRenderTargetView.Get(); }
        ID3D11RenderTargetView* GetPickingRenderTargetView() const noexcept { return m_d3dPickingRenderTargetView.Get(); }
        ID3D11RenderTargetView* GetOffscreenRenderTargetView() const noexcept { return m_d3dOffscreenRenderTargetView.Get(); }
        ID3D11DepthStencilView* GetDepthStencilView() const noexcept { return m_d3dDepthStencilView.Get(); }
        ID3D11DepthStencilView* GetOffscreenDepthStencilView() const noexcept { return m_d3dOffscreenDepthStencilView.Get(); }
        DXGI_FORMAT             GetBackBufferFormat() const noexcept    { return m_backBufferFormat; }
        DXGI_FORMAT             GetDepthBufferFormat() const noexcept   { return m_depthBufferFormat; }
        D3D11_VIEWPORT          GetScreenViewport() const noexcept      { return m_screenViewport; }
        D3D11_VIEWPORT          GetOffscreenViewport() const noexcept { return m_offscreenViewport; }
        UINT                    GetBackBufferCount() const noexcept     { return m_backBufferCount; }
        DXGI_COLOR_SPACE_TYPE   GetColorSpace() const noexcept          { return m_colorSpace; }
        unsigned int            GetDeviceOptions() const noexcept       { return m_options; }

        // Performance events
        void PIXBeginEvent(_In_z_ const wchar_t* name)
        {
            m_d3dAnnotation->BeginEvent(name);
        }

        void PIXEndEvent()
        {
            m_d3dAnnotation->EndEvent();
        }

        void PIXSetMarker(_In_z_ const wchar_t* name)
        {
            m_d3dAnnotation->SetMarker(name);
        }

        std::vector<std::pair<int, int>> GetMsaaLevels() {
            return m_mssa_levels;
        }

        int GetMsaaLevelIndex() {
            return m_current_mssa_level_index;
        }

        void SetMsaaLevel(int mssa_level_index) {
            if (mssa_level_index < m_mssa_levels.size()) {
                m_swapChain.Reset();
                m_current_mssa_level_index = mssa_level_index;
            }

            CreateWindowSizeDependentResources();
        }

    private:
        void CreateFactory();
        void GetHardwareAdapter(IDXGIAdapter1** ppAdapter);

        // Direct3D objects.
        Microsoft::WRL::ComPtr<IDXGIFactory2>               m_dxgiFactory;
        Microsoft::WRL::ComPtr<ID3D11Device1>               m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext1>        m_d3dContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain1>             m_swapChain;
        Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation>   m_d3dAnnotation;

        std::vector<std::pair<int, int>> m_mssa_levels; // pair(sampleCount, maxQualityLevel)
        int m_current_mssa_level_index = 0;

        // Direct3D rendering objects. Required for 3D.
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_renderTarget;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_pickingRenderTarget;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_pickingStagingTexture;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_pickingNonMsaaTexture;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_offscreenRenderTarget;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_offscreenStagingTexture;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_offscreenNonMsaaTexture;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_depthStencil;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_offscreenDepthStencil;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_d3dRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_d3dPickingRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_d3dOffscreenRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_d3dDepthStencilView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_d3dOffscreenDepthStencilView;
        D3D11_VIEWPORT                                  m_screenViewport;
        D3D11_VIEWPORT                                  m_offscreenViewport;

        // Direct3D properties.
        DXGI_FORMAT                                     m_backBufferFormat;
        DXGI_FORMAT                                     m_depthBufferFormat;
        UINT                                            m_backBufferCount;
        D3D_FEATURE_LEVEL                               m_d3dMinFeatureLevel;

        // Cached device properties.
        HWND                                            m_window;
        D3D_FEATURE_LEVEL                               m_d3dFeatureLevel;
        RECT                                            m_outputSize;

        // HDR Support
        DXGI_COLOR_SPACE_TYPE                           m_colorSpace;

        // DeviceResources options (see flags above)
        unsigned int                                    m_options;

        // The IDeviceNotify can be held directly as it owns the DeviceResources.
        IDeviceNotify*                                  m_deviceNotify;
    };
}
