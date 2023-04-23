#include "pch.h"
#include "TextureManager.h"
#include <wincodec.h>

HRESULT TextureManager::CreateTextureFromDDSInMemory(const uint8_t* ddsData, size_t ddsDataSize,
                                                     int* textureID_out, int* width_out, int* height_out,
                                                     std::vector<RGBA>& rgba_data_out, int file_hash)
{
    if (cached_textures.contains(file_hash))
        return cached_textures[file_hash];

    if (! ddsData)
    {
        return E_FAIL;
    }

    DirectX::TexMetadata metadata;
    DirectX::ScratchImage image;

    HRESULT hr = DirectX::LoadFromDDSMemory(ddsData, ddsDataSize, DirectX::DDS_FLAGS_NONE, &metadata, image);
    if (SUCCEEDED(hr))
    {
        if (metadata.height <= 0 || metadata.width <= 0)
        {
            return E_FAIL;
        }

        DXGI_FORMAT targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        if (metadata.format != targetFormat)
        {
            // Check if the format is a block compressed format
            if (DirectX::IsCompressed(metadata.format))
            {
                // Decompress the compressed format
                DirectX::ScratchImage decompressedImage;
                hr = DirectX::Decompress(*image.GetImages(), targetFormat, decompressedImage);
                if (SUCCEEDED(hr))
                {
                    image.Release();
                    image = std::move(decompressedImage);
                    metadata.format = decompressedImage.GetMetadata().format;
                }
            }

            // Convert the format to DXGI_FORMAT_R8G8B8A8_UNORM
            if (metadata.format != targetFormat)
            {
                DirectX::ScratchImage convertedImage;
                hr = DirectX::Convert(*image.GetImages(), targetFormat, DirectX::TEX_FILTER_DEFAULT,
                                      DirectX::TEX_THRESHOLD_DEFAULT, convertedImage);
                if (SUCCEEDED(hr))
                {
                    image.Release();
                    image = std::move(convertedImage);
                    metadata.format = targetFormat;
                }
            }
        }

        DXGI_FORMAT format = metadata.format;
        UINT width = static_cast<UINT>(metadata.width);
        UINT height = static_cast<UINT>(metadata.height);

        const DirectX::Image* img = image.GetImages();
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = img->pixels;
        initData.SysMemPitch = static_cast<UINT>(img->rowPitch);
        initData.SysMemSlicePitch = static_cast<UINT>(img->slicePitch);

        *textureID_out = AddTexture(initData.pSysMem, width, height, format, file_hash);
        *width_out = width;
        *height_out = height;

        // Extract RGBA data
        size_t pixel_data_size = width * height * 4;
        rgba_data_out.resize(pixel_data_size / sizeof(RGBA));
        memcpy(rgba_data_out.data(), img->pixels, pixel_data_size);

        return (*textureID_out >= 0) ? S_OK : E_FAIL;
    }
    else
    {
        // Handle the error
        return hr;
    }
}

HRESULT TextureManager::SaveTextureToFile(ID3D11ShaderResourceView* srv, const wchar_t* filename)
{
    HRESULT hr = S_OK;

    // Get the resource from the shader resource view
    ID3D11Resource* resource = nullptr;
    srv->GetResource(&resource);

    // Query the resource for the ID3D11Texture2D interface
    ID3D11Texture2D* texture = nullptr;
    hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
    if (FAILED(hr))
    {
        resource->Release();
        return hr;
    }

    // Get the texture description
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture2D* stagingTexture;
    hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr))
    {
        return hr;
    }

    // Copy the data to the staging texture
    m_deviceContext->CopyResource(stagingTexture, texture);

    // Map the staging texture and read the pixel data
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_deviceContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr))
    {
        stagingTexture->Release();
        return hr;
    }

    // Create an image from the pixel data
    DirectX::Image image;
    image.width = desc.Width;
    image.height = desc.Height;
    image.format = stagingDesc.Format;
    image.rowPitch = mappedResource.RowPitch;
    image.slicePitch = image.rowPitch * image.height;
    image.pixels = static_cast<uint8_t*>(mappedResource.pData);

    // Save the image to a file
    hr = DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
                                filename);

    // Unmap and release the staging texture
    m_deviceContext->Unmap(stagingTexture, 0);
    stagingTexture->Release();

    // Release the texture and resource
    texture->Release();
    resource->Release();

    return hr;
}
DatTexture TextureManager::BuildTextureAtlas(const std::vector<DatTexture>& terrain_dat_textures,
                                             int num_cols, int num_rows)
{
    // Check if the input vector is empty
    if (terrain_dat_textures.empty())
    {
        return {};
    }

    // Find the maximum dimensions among all textures
    int maxTexWidth = 0;
    int maxTexHeight = 0;

    for (const auto& texture : terrain_dat_textures)
    {
        maxTexWidth = std::max(maxTexWidth, texture.width);
        maxTexHeight = std::max(maxTexHeight, texture.height);
    }

    // If either num_cols or num_rows is -1, compute them automatically
    if (num_cols == -1 || num_rows == -1)
    {
        int numTextures = static_cast<int>(terrain_dat_textures.size());
        num_cols = static_cast<int>(std::ceil(std::sqrt(numTextures)));
        num_rows = static_cast<int>(std::ceil(static_cast<double>(numTextures) / num_cols));
    }

    unsigned int atlasWidth = maxTexWidth * num_cols;
    unsigned int atlasHeight = maxTexHeight * num_rows;

    std::vector<RGBA> atlasData(atlasWidth * atlasHeight, {0, 0, 0, 0});

    int numTextures = static_cast<int>(terrain_dat_textures.size());

    for (int row = 0; row < num_rows; ++row)
    {
        for (int col = 0; col < num_cols; ++col)
        {
            int textureIndex = row * num_cols + col;

            if (textureIndex < numTextures)
            {
                const auto& texture = terrain_dat_textures[textureIndex];
                const std::vector<RGBA>& textureData = texture.rgba_data;
                int texWidth = texture.width;
                int texHeight = texture.height;

                for (int y = 0; y < texHeight; ++y)
                {
                    for (int x = 0; x < texWidth; ++x)
                    {
                        int atlasX = col * maxTexWidth + x;
                        int atlasY = row * maxTexHeight + y;

                        int atlasOffset = atlasY * atlasWidth + atlasX;
                        int textureOffset = y * texWidth + x;

                        atlasData[atlasOffset] = textureData[textureOffset];
                    }
                }
            }
        }
    }

    DatTexture terrain_atlas;
    terrain_atlas.width = atlasWidth;
    terrain_atlas.height = atlasHeight;
    terrain_atlas.rgba_data = std::move(atlasData);

    return terrain_atlas;
}
