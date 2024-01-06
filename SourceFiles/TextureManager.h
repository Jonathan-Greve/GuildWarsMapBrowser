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

struct TextureData
{
	int textureID;
	int width;
	int height;
	std::vector<RGBA> rgba_data;
};

class TextureManager
{
public:
	TextureManager(ID3D11Device* device, ID3D11DeviceContext* device_context)
		: m_device(device)
		  , m_deviceContext(device_context) { }

	~TextureManager() { Clear(); }

	int AddTexture(const void* data, UINT width, UINT height, DXGI_FORMAT format, int file_hash,
	               bool autoGenerateMipMaps = true)
	{
		if (cached_textures.contains(file_hash))
			return cached_textures[file_hash].textureID;

		if (!data || width <= 0 || height <= 0) { return -1; }

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = autoGenerateMipMaps ? 0 : 1;
		texDesc.ArraySize = 1;
		texDesc.Format = format;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (autoGenerateMipMaps ? D3D11_BIND_RENDER_TARGET : 0);
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = autoGenerateMipMaps ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

		D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
		D3D11_SUBRESOURCE_DATA initData = {};
		if (!autoGenerateMipMaps)
		{
			initData.pSysMem = data;
			UINT bytesPerPixel = BytesPerPixel(format);
			initData.SysMemPitch = width * bytesPerPixel;
			initData.SysMemSlicePitch = width * height * bytesPerPixel;
			pInitData = &initData;
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
		HRESULT hr = m_device->CreateTexture2D(&texDesc, pInitData, texture2D.GetAddressOf());
		if (FAILED(hr)) { return -1; }

		if (autoGenerateMipMaps)
		{
			UINT bytesPerPixel = BytesPerPixel(format);
			m_deviceContext->UpdateSubresource(texture2D.Get(), 0, nullptr, data, width * bytesPerPixel, 0);
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = autoGenerateMipMaps ? -1 : 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
		hr = m_device->CreateShaderResourceView(texture2D.Get(), &srvDesc, shaderResourceView.GetAddressOf());
		if (FAILED(hr)) { return -1; }

		if (autoGenerateMipMaps) { m_deviceContext->GenerateMips(shaderResourceView.Get()); }

		int textureID = m_nextTextureID++;
		m_textures[textureID] = shaderResourceView;

		if (file_hash >= 0)
		{
			TextureData textureData;
			textureData.textureID = textureID;
			textureData.width = width;
			textureData.height = height;

			UINT bytesPerPixel = BytesPerPixel(format);
			textureData.rgba_data.reserve(width * height); // Reserve space for efficiency

			const unsigned char* byteData = static_cast<const unsigned char*>(data);

			for (int i = 0; i < width * height * bytesPerPixel; i += bytesPerPixel)
			{
				RGBA color;
				color.r = byteData[i];
				color.g = byteData[i + 1];
				color.b = byteData[i + 2];
				color.a = byteData[i + 3];
				textureData.rgba_data.push_back(color);
			}

			cached_textures[file_hash] = textureData;
		}

		return textureID;
	}

	int AddTextureArray(const std::vector<void*>& dataArray, UINT width, UINT height, DXGI_FORMAT format, int file_hash,
	                    bool autoGenerateMipMaps = true)
	{
		if (!dataArray.size() || width <= 0 || height <= 0) { return -1; }

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = autoGenerateMipMaps ? std::floor(std::log2(std::max(width, height))) + 1 : 1;
		texDesc.ArraySize = dataArray.size();
		texDesc.Format = format;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (autoGenerateMipMaps ? D3D11_BIND_RENDER_TARGET : 0);
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = autoGenerateMipMaps ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
		HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, texture2D.GetAddressOf());
		if (FAILED(hr)) { return -1; }

		UINT bytesPerPixel = BytesPerPixel(format); // Assuming you have this function

		// 1. Upload the original (mip level 0) texture data
    for (size_t i = 0; i < dataArray.size(); ++i)
    {
        m_deviceContext->UpdateSubresource(
            texture2D.Get(),
            D3D11CalcSubresource(0, i, texDesc.MipLevels),
            nullptr,
            dataArray[i],
            width * bytesPerPixel,
            0
        );
    }


		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.ArraySize = dataArray.size();
		srvDesc.Texture2DArray.MipLevels = autoGenerateMipMaps ? -1 : 1;
		srvDesc.Texture2DArray.MostDetailedMip = 0;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
		hr = m_device->CreateShaderResourceView(texture2D.Get(), &srvDesc, shaderResourceView.GetAddressOf());
		if (FAILED(hr)) { return -1; }

		    // 2. If auto-generation of mipmaps is enabled, use the hardware-accelerated GenerateMips method
    if (autoGenerateMipMaps)
    {
        // Ensure we have a valid shader resource view
        m_deviceContext->GenerateMips(shaderResourceView.Get());
    }

		int textureID = m_nextTextureID++;
		m_textures[textureID] = shaderResourceView;

		if (file_hash >= 0)
		{
			TextureData textureData;
			textureData.textureID = textureID;
			textureData.width = width;
			textureData.height = height;
			cached_textures[file_hash] = textureData;
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

		if (it != m_textures.end()) { return it->second.Get(); }
		return nullptr;
	}

	std::optional<TextureData> GetTextureDataByHash(int file_hash) const
	{
		auto it = cached_textures.find(file_hash);

		if (it != cached_textures.end()) { return it->second; }
		return std::nullopt;
	}

	int GetTextureIdByHash(int file_hash) const
	{
		auto it = cached_textures.find(file_hash);

		if (it != cached_textures.end()) { return it->second.textureID; }
		return -1;
	}

	std::vector<ID3D11ShaderResourceView*> GetTextures(const std::vector<int>& textureIDs) const
	{
		std::vector<ID3D11ShaderResourceView*> textures;
		textures.reserve(textureIDs.size());

		for (const int textureID : textureIDs)
		{
			ID3D11ShaderResourceView* texture = GetTexture(textureID);
			if (texture) { textures.push_back(texture); }
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
			*textureID = AddTexture(data, width, height, format, file_hash);
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
	std::unordered_map<int, TextureData> cached_textures;

	std::unordered_map<int, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textures;

	void GenerateMipmapLevel(const std::vector<uint8_t>& higherLevelData,
                         std::vector<uint8_t>& lowerLevelData,
                         UINT higherWidth, UINT higherHeight, UINT bytesPerPixel)
{
    if (bytesPerPixel != 4)
    {
        return; // Unsupported format
    }

    UINT lowerWidth = higherWidth / 2;
    UINT lowerHeight = higherHeight / 2;
    lowerLevelData.resize(lowerWidth * lowerHeight * bytesPerPixel);

    const uint8_t* src = higherLevelData.data();
    uint8_t* dst = lowerLevelData.data();

    for (UINT y = 0; y < lowerHeight; ++y)
    {
        for (UINT x = 0; x < lowerWidth; ++x)
        {
            UINT dstIdx = 4 * (y * lowerWidth + x);

            for (UINT channel = 0; channel < 4; ++channel)
            {
                std::vector<uint8_t> values;
                for (int dy = -1; dy <= 1; dy++)
                {
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        int nx = 2 * x + dx;
                        int ny = 2 * y + dy;

                        if (nx < 0 || ny < 0 || nx >= higherWidth || ny >= higherHeight)
                            continue;

                        UINT srcIdx = 4 * (ny * higherWidth + nx);
                        values.push_back(src[srcIdx + channel]);
                    }
                }

                // Sort and find the median
                std::sort(values.begin(), values.end());
                dst[dstIdx + channel] = values[values.size() / 2];
            }
        }
    }
}

};

inline bool SaveTextureToPng(ID3D11ShaderResourceView* texture, std::wstring& filename,
	TextureManager* texture_manager)
{
	HRESULT hr = texture_manager->SaveTextureToFile(texture, filename.c_str());
	if (FAILED(hr))
	{
		// Handle the error
		return false;
	}

	return true;
}

inline bool SaveTextureToDDS(const TextureData& textureData, const std::wstring& filename)
{
	// Assuming each RGBA value is stored as 4 consecutive bytes
	size_t totalSize = textureData.rgba_data.size() * sizeof(RGBA);
	std::vector<uint8_t> pixelData(totalSize);

	// Copy the pixel data
	std::memcpy(pixelData.data(), textureData.rgba_data.data(), totalSize);

	// Create the Image structure
	DirectX::Image image;
	image.width = static_cast<size_t>(textureData.width);
	image.height = static_cast<size_t>(textureData.height);
	image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	image.rowPitch = textureData.width * sizeof(RGBA);
	image.slicePitch = image.rowPitch * textureData.height;
	image.pixels = pixelData.data();

	HRESULT hr = DirectX::SaveToDDSFile(image, DirectX::DDS_FLAGS_NONE, filename.c_str());

	return SUCCEEDED(hr);
}