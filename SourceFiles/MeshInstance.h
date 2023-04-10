#pragma once
#include "Mesh.h"
#include "PerObjectCB.h"

class MeshInstance
{
public:
    MeshInstance(ID3D11Device* device, Mesh mesh, int mesh_id)
        : m_mesh_id(mesh_id)
    {
        m_mesh = mesh;

        // Create vertex buffer
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.ByteWidth = sizeof(GWVertex) * m_mesh.vertices.size();
        vbDesc.StructureByteStride = sizeof(GWVertex);
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

    ~MeshInstance() { }

    int GetMeshID() const { return m_mesh_id; }

    const PerObjectCB& GetPerObjectData() const { return m_per_object_data; }
    void SetPerObjectData(const PerObjectCB& data) { m_per_object_data = data; }

    void SetTextures(const std::vector<ID3D11ShaderResourceView*>& textures)
    {
        if (textures.size() >= 32)
        {
            return; // Failed, maybe throw here on handle error.
        }

        m_textures.clear();
        for (size_t i = 0; i < textures.size(); ++i)
        {
            // Add the texture to the corresponding uvSetIndex
            m_textures.push_back(textures[i]);
        }
    }

    const Mesh& GetMesh() { return m_mesh; }

    void Draw(ID3D11DeviceContext* context)
    {
        UINT stride = sizeof(GWVertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        context->PSSetShaderResources(0, m_textures.size(), m_textures.data()->GetAddressOf());

        context->DrawIndexed(m_mesh.indices.size(), 0, 0);
    }

private:
    Mesh m_mesh;
    int m_mesh_id;
    PerObjectCB m_per_object_data;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textures;
};
