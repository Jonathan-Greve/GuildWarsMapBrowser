class ConstantBufferManager
{
public:
    ConstantBufferManager(ID3D11Device* device)
        : m_device(device)
    {
    }

    template <typename T> Microsoft::WRL::ComPtr<ID3D11Buffer> CreateConstantBuffer()
    {
        D3D11_BUFFER_DESC bufferDesc;
        ZeroMemory(&bufferDesc, sizeof(bufferDesc));
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.ByteWidth = sizeof(T);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
        HRESULT hr = m_device->CreateBuffer(&bufferDesc, nullptr, &constantBuffer);
        if (FAILED(hr))
        {
            return nullptr;
        }

        return constantBuffer;
    }

    template <typename T>
    void UpdateConstantBuffer(ID3D11DeviceContext* context, ID3D11Buffer* constantBuffer, const T& data)
    {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            memcpy(mappedResource.pData, &data, sizeof(T));
            context->Unmap(constantBuffer, 0);
        }
    }

private:
    ID3D11Device* m_device;
};
