#pragma once
#include "Mesh.h"

class MeshInstance
{
public:
    MeshInstance(ID3D11Device* device, Mesh mesh)
    {
        m_mesh = mesh;

        // Create vertex buffer
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.ByteWidth = sizeof(Vertex) * m_mesh.vertices.size();
        vbDesc.StructureByteStride = sizeof(Vertex);
        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = m_mesh.vertices.data();
        device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);

        // Create index buffer
        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.ByteWidth = sizeof(uint32_t) * m_mesh.indices.size();
        ibDesc.StructureByteStride = sizeof(uint32_t);
        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = m_mesh.indices.data();
        device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    }

    ~MeshInstance()
    {
        if (m_vertexBuffer)
            m_vertexBuffer->Release();
        if (m_indexBuffer)
            m_indexBuffer->Release();
    }

    void Draw(ID3D11DeviceContext* context)
    {
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexed(m_mesh.indices.size(), 0, 0);
    }

private:
    Mesh m_mesh;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
};
