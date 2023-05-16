#pragma once

enum class BlendState
{
    Opaque,
    AlphaBlend,
    Additive,
    Multiplicative,
    Screen,
    Subtractive
};

class BlendStateManager
{
public:
    BlendStateManager(ID3D11Device* device, ID3D11DeviceContext* device_context)
        : m_device(device)
        , m_deviceContext(device_context)
    {
        CreateBlendState(device, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, &m_opaque_blend_state); // Opaque
        CreateBlendState(device, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
                         &m_alpha_blend_state); // Alpha Blend
        CreateBlendState(device, D3D11_BLEND_ONE, D3D11_BLEND_ONE, &m_additive_blend_state); // Additive
        CreateBlendState(device, D3D11_BLEND_ZERO, D3D11_BLEND_SRC_COLOR,
                         &m_multiplicative_blend_state); // Multiplicative
        CreateBlendState(device, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_COLOR, &m_screen_blend_state); // Screen
        CreateBlendState(device, D3D11_BLEND_ONE, D3D11_BLEND_ONE, &m_subtractive_blend_state,
                         D3D11_BLEND_OP_REV_SUBTRACT); // Subtractive
    }

    ID3D11BlendState* GetBlendState(BlendState blendState)
    {
        switch (blendState)
        {
        case BlendState::Opaque:
            return m_opaque_blend_state.Get();
        case BlendState::AlphaBlend:
            return m_alpha_blend_state.Get();
        case BlendState::Additive:
            return m_additive_blend_state.Get();
        case BlendState::Multiplicative:
            return m_multiplicative_blend_state.Get();
        case BlendState::Screen:
            return m_screen_blend_state.Get();
        case BlendState::Subtractive:
            return m_subtractive_blend_state.Get();
        default:
            return nullptr;
        }
    }

    void SetBlendState(BlendState blendState, const float blendFactor[] = nullptr,
                       UINT sampleMask = 0xFFFFFFFF)
    {
        ID3D11BlendState* blendStatePtr = GetBlendState(blendState);

        if (blendFactor == nullptr)
        {
            static float defaultBlendFactor[] = {0.0f, 0.0f, 0.0f, 0.0f};
            m_deviceContext->OMSetBlendState(blendStatePtr, defaultBlendFactor, sampleMask);
        }
        else
        {
            m_deviceContext->OMSetBlendState(blendStatePtr, blendFactor, sampleMask);
        }
    }

    void ResetBlendState()
    {
        static float defaultBlendFactor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        m_deviceContext->OMSetBlendState(nullptr, defaultBlendFactor, 0xFFFFFFFF);
    }

private:
    HRESULT CreateBlendState(ID3D11Device* device, D3D11_BLEND srcBlend, D3D11_BLEND destBlend,
                             Microsoft::WRL::ComPtr<ID3D11BlendState>* blendState,
                             D3D11_BLEND_OP blendOp = D3D11_BLEND_OP_ADD)
    {
        D3D11_BLEND_DESC blendDesc;
        ZeroMemory(&blendDesc, sizeof(blendDesc));

        blendDesc.RenderTarget[0].BlendEnable =
          (srcBlend != D3D11_BLEND_ONE) || (destBlend != D3D11_BLEND_ZERO);
        blendDesc.RenderTarget[0].SrcBlend = srcBlend;
        blendDesc.RenderTarget[0].DestBlend = destBlend;
        blendDesc.RenderTarget[0].BlendOp = blendOp;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        return device->CreateBlendState(&blendDesc, blendState->GetAddressOf());
    }

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;

    Microsoft::WRL::ComPtr<ID3D11BlendState> m_opaque_blend_state;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_alpha_blend_state;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_additive_blend_state;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_multiplicative_blend_state;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_screen_blend_state;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_subtractive_blend_state;
};
