#pragma once
#include <d3d11.h>
#include <wrl/client.h>

enum class RasterizerStateType
{
    Solid,
    Solid_NoCull,
    Wireframe,
    Wireframe_NoCull
};

class RasterizerStateManager
{
public:
    RasterizerStateManager(Microsoft::WRL::ComPtr<ID3D11Device> device,
                           Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext)
        : m_device(device)
        , m_deviceContext(deviceContext)
    {
        CreateRasterizerStates();
    }

    void SetRasterizerState(RasterizerStateType state)
    {
        m_currentRasterizerState = state;
        switch (m_currentRasterizerState)
        {
        case RasterizerStateType::Solid:
            m_deviceContext->RSSetState(m_solid_rs.Get());
            break;
        case RasterizerStateType::Solid_NoCull:
            m_deviceContext->RSSetState(m_solid_no_cull_rs.Get());
            break;
        case RasterizerStateType::Wireframe:
            m_deviceContext->RSSetState(m_wireframe_rs.Get());
            break;
        case RasterizerStateType::Wireframe_NoCull:
            m_deviceContext->RSSetState(m_wireframe_no_cull_rs.Get());
            break;
        }
    }

    RasterizerStateType GetCurrentRasterizerState() const { return m_currentRasterizerState; }

private:
    void CreateRasterizerStates()
    {
        D3D11_RASTERIZER_DESC rsDesc;
        ZeroMemory(&rsDesc, sizeof(D3D11_RASTERIZER_DESC));
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE;
        rsDesc.FrontCounterClockwise = false;
        rsDesc.DepthClipEnable = true;

        HRESULT hr = m_device->CreateRasterizerState(&rsDesc, m_solid_no_cull_rs.GetAddressOf());
        if (FAILED(hr))
            throw "Failed creating solid frame rasterizer state.";
        rsDesc.FillMode = D3D11_FILL_WIREFRAME;
        hr = m_device->CreateRasterizerState(&rsDesc, m_wireframe_no_cull_rs.GetAddressOf());
        if (FAILED(hr))
            throw "Failed creating solid frame rasterizer state.";
        rsDesc.CullMode = D3D11_CULL_BACK;
        hr = m_device->CreateRasterizerState(&rsDesc, m_wireframe_rs.GetAddressOf());
        if (FAILED(hr))
            throw "Failed creating solid frame rasterizer state.";
        rsDesc.FillMode = D3D11_FILL_SOLID;
        hr = m_device->CreateRasterizerState(&rsDesc, m_solid_rs.GetAddressOf());
        if (FAILED(hr))
            throw "Failed creating solid frame rasterizer state.";
    }

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_deviceContext;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_wireframe_rs;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_wireframe_no_cull_rs;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_solid_rs;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_solid_no_cull_rs;

    RasterizerStateType m_currentRasterizerState = RasterizerStateType::Solid;
};
