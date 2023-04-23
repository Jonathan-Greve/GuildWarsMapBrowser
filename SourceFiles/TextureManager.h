#pragma once
#include "AtexReader.h"
#include "DirectXTex/DirectXTex.h"

inline UINT BytesPerPixel(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SINT:
        return 1;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return 4;
    default:
        return 0; // Return 0 for unsupported formats
    }
}

class TextureManager
{
public:
    TextureManager(ID3D11Device* device, ID3D11DeviceContext* device_context)
        : m_device(device)
        , m_deviceContext(device_context)
    {
    }

    ~TextureManager() { Clear(); }

    int AddTexture(const void* data, UINT width, UINT height, DXGI_FORMAT format, int file_hash)
    {
        if (cached_textures.contains(file_hash))
            return cached_textures[file_hash];

        if (! data || width <= 0 || height <= 0)
        {
            return -1;
        }

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
        UINT bytesPerPixel = BytesPerPixel(format);
        initData.SysMemPitch = static_cast<UINT>(width * bytesPerPixel);
        initData.SysMemSlicePitch = static_cast<UINT>(width * height * bytesPerPixel);

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

        int textureID = m_nextTextureID++;
        m_textures[textureID] = shaderResourceView;

        if (file_hash >= 0)
        {
            cached_textures[file_hash] = textureID;
        }

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

    int GetTextureIdByHash(int file_hash) const
    {
        auto it = cached_textures.find(file_hash);

        if (it != cached_textures.end())
        {
            return it->second;
        }
        return -1;
    }

    std::vector<ID3D11ShaderResourceView*> GetTextures(const std::vector<int>& textureIDs) const
    {
        std::vector<ID3D11ShaderResourceView*> textures;
        textures.reserve(textureIDs.size());

        for (const int textureID : textureIDs)
        {
            ID3D11ShaderResourceView* texture = GetTexture(textureID);
            if (texture)
            {
                textures.push_back(texture);
            }
            else
            {
                // Handle the case when a texture is not found, if necessary
            }
        }

        return textures;
    }

    HRESULT CreateTextureFromRGBA(int width, int height, const RGBA* data, int* textureID, int file_hash)
    {
        if (width >= 0 && height >= 0)
        {
            DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
            *textureID = AddTexture((void*)data, width, height, format, file_hash);
            return (*textureID >= 0) ? S_OK : E_FAIL;
        }
        return E_FAIL;
    }

    HRESULT CreateTextureFromDDSInMemory(const uint8_t* ddsData, size_t ddsDataSize, int* textureID_out,
                                         int* width_out, int* height_out, std::vector<RGBA>& rgba_data_out,
                                         int file_hash);
    HRESULT SaveTextureToFile(ID3D11ShaderResourceView* srv, const wchar_t* filename);

    DatTexture BuildTextureAtlas(const std::vector<DatTexture>& terrain_dat_textures, int num_cols,
                                 int num_rows);

    void Clear() { m_textures.clear(); }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    int m_nextTextureID = 0;
    std::unordered_map<int, int> cached_textures; // file hash -> texture id;
    std::unordered_map<int, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textures;
};
