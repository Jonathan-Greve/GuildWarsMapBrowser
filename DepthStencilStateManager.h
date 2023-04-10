#pragma once
#include <array>

enum class DepthStencilStateType
{
    Enabled,
    Disabled
};

class DepthStencilStateManager
{
public:
    DepthStencilStateManager(ID3D11Device* device, ID3D11DeviceContext* device_context)
        : m_device(device)
        , m_deviceContext(device_context)
    {
        // Create depth stencil state for Enabled
        D3D11_DEPTH_STENCIL_DESC dsDescEnabled;
        ZeroMemory(&dsDescEnabled, sizeof(dsDescEnabled));
        dsDescEnabled.DepthEnable = true;
        dsDescEnabled.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDescEnabled.DepthFunc = D3D11_COMPARISON_LESS;

        ID3D11DepthStencilState* pDSStateEnabled;
        m_device->CreateDepthStencilState(&dsDescEnabled, &pDSStateEnabled);
        m_depthStencilStates[static_cast<size_t>(DepthStencilStateType::Enabled)] = pDSStateEnabled;

        // Create depth stencil state for Disabled
        D3D11_DEPTH_STENCIL_DESC dsDescDisabled;
        ZeroMemory(&dsDescDisabled, sizeof(dsDescDisabled));
        dsDescDisabled.DepthEnable = false;
        dsDescDisabled.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDescDisabled.DepthFunc = D3D11_COMPARISON_LESS;

        ID3D11DepthStencilState* pDSStateDisabled;
        m_device->CreateDepthStencilState(&dsDescDisabled, &pDSStateDisabled);
        m_depthStencilStates[static_cast<size_t>(DepthStencilStateType::Disabled)] = pDSStateDisabled;
    }

    ~DepthStencilStateManager()
    {
        for (auto state : m_depthStencilStates)
        {
            if (state)
            {
                state->Release();
            }
        }
    }

    void SetDepthStencilState(DepthStencilStateType type)
    {
        m_deviceContext->OMSetDepthStencilState(m_depthStencilStates[static_cast<size_t>(type)], 1);
    }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;

    std::array<ID3D11DepthStencilState*, 2> m_depthStencilStates;
};
