#pragma once
#include "AtexReader.h"
#include "DirectXTex/DirectXTex.h"

class TextureManager
{
public:
    TextureManager(ID3D11Device* device, ID3D11DeviceContext* device_context)
        : m_device(device)
        , m_deviceContext(device_context)
    {
    }

    ~TextureManager() { Clear(); }

    int AddTexture(const void* data, UINT width, UINT height, DXGI_FORMAT format)
    {
        int textureID = m_nextTextureID++;

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = format;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = data;
        initData.SysMemPitch = static_cast<UINT>(width * 4);
        initData.SysMemSlicePitch = static_cast<UINT>(width * height * 4);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
        HRESULT hr = m_device->CreateTexture2D(&texDesc, &initData, texture2D.GetAddressOf());
        if (FAILED(hr))
        {
            return -1;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
        hr = m_device->CreateShaderResourceView(texture2D.Get(), &srvDesc, shaderResourceView.GetAddressOf());
        if (FAILED(hr))
        {
            return -1;
        }

        m_textures[textureID] = shaderResourceView;
        return textureID;
    }

    bool RemoveTexture(int textureID)
    {
        auto it = m_textures.find(textureID);
        if (it != m_textures.end())
        {
            m_textures.erase(it);
            return true;
        }
        return false;
    }

    ID3D11ShaderResourceView* GetTexture(int textureID) const
    {
        auto it = m_textures.find(textureID);

        if (it != m_textures.end())
        {
            return it->second.Get();
        }
        return nullptr;
    }

    HRESULT CreateTextureFromRGBA(int width, int height, const RGBA* data, int* textureID)
    {
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        *textureID = AddTexture((void*)data, width, height, format);
        return (*textureID >= 0) ? S_OK : E_FAIL;
    }

    HRESULT CreateTextureFromDDSInMemory(const uint8_t* ddsData, size_t ddsDataSize, int* textureID_out,
                                         int* width_out, int* height_out, std::vector<RGBA>& rgba_data_out);
    HRESULT SaveTextureToFile(ID3D11ShaderResourceView* srv, const wchar_t* filename);

    void Clear() { m_textures.clear(); }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    int m_nextTextureID = 0;
    std::unordered_map<int, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textures;
};
