#pragma once
#include "Mesh.h"
#include "PerObjectCB.h"
#include <array>

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
        ibDesc.StructureByteStride = sizeof(uint32_t);

        // High LOD indices
        ibDesc.ByteWidth = sizeof(uint32_t) * m_mesh.indices.size();
        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = m_mesh.indices.data();
        device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer_high);

        // Medium LOD indices
        if (m_mesh.indices1.size() > 0) {
            ibDesc.ByteWidth = sizeof(uint32_t) * m_mesh.indices1.size();
            ibData.pSysMem = m_mesh.indices1.data();
            device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer_medium);
        }

        // Low LOD indices
        if (m_mesh.indices2.size() > 0) {
            ibDesc.ByteWidth = sizeof(uint32_t) * m_mesh.indices2.size();
            ibData.pSysMem = m_mesh.indices2.data();
            device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer_low);
        }
    }

    ~MeshInstance() { }

    int GetMeshID() const { return m_mesh_id; }

    const PerObjectCB& GetPerObjectData() const { return m_per_object_data; }
    void SetPerObjectData(const PerObjectCB& data) { m_per_object_data = data; }

    void SetTextures(const std::vector<ID3D11ShaderResourceView*>& textures, int slot)
    {
        assert(slot < 4); // Ensure slot is 4 at most (increase if neccessary)

        if (textures.size() >= MAX_NUM_TEX_INDICES)
        {
            return; // Failed, maybe throw here or handle error.
        }

        m_textures[slot].clear();
        for (size_t i = 0; i < textures.size(); ++i)
        {
            // Add the texture to the corresponding slot
            m_textures[slot].push_back(textures[i]);
        }
    }

    const Mesh& GetMesh() { return m_mesh; }

    void UpdateVertices(ID3D11Device* device, const std::vector<GWVertex>& vertices) {
        if (vertices.empty()) return;

        m_mesh.vertices = vertices;

        // Recreate vertex buffer with new data
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.ByteWidth = sizeof(GWVertex) * m_mesh.vertices.size();
        vbDesc.StructureByteStride = sizeof(GWVertex);
        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = m_mesh.vertices.data();

        m_vertexBuffer.Reset();
        device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
    }

    void SetShouldCull(const bool should_cull) {
        m_mesh.should_cull = should_cull;
    }

    void Draw(ID3D11DeviceContext* context, LODQuality lod_quality)
    {
        UINT stride = sizeof(GWVertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

        int num_indices = 0;

        switch (lod_quality)
        {
        case LODQuality::High:
            context->IASetIndexBuffer(m_indexBuffer_high.Get(), DXGI_FORMAT_R32_UINT, 0);
            num_indices = m_mesh.indices.size();
            break;
        case LODQuality::Medium:
            // Fallthrough to next case if indices are empty
            if (m_mesh.indices1.size() > 0) {
                context->IASetIndexBuffer(m_indexBuffer_medium.Get(), DXGI_FORMAT_R32_UINT, 0);
                num_indices = m_mesh.indices1.size();
                break;
            }
        case LODQuality::Low:
            // Fallthrough to next case if indices are empty
            if (m_mesh.indices2.size() > 0) {
                context->IASetIndexBuffer(m_indexBuffer_low.Get(), DXGI_FORMAT_R32_UINT, 0);
                num_indices = m_mesh.indices2.size();
                break;
            }
        default:
            context->IASetIndexBuffer(m_indexBuffer_high.Get(), DXGI_FORMAT_R32_UINT, 0);
            num_indices = m_mesh.indices.size();
            break;
        }

        // Bind all texture ranges first, then clear only slots that are not covered by
        // any binding. This preserves multi-SRV ranges (for example water t0+t1).
        bool covered_slots[4] = { false, false, false, false };
        for (int slot = 0; slot < 4; ++slot)
        {
            const auto srv_count = static_cast<UINT>(m_textures[slot].size());
            if (srv_count > 0)
            {
                // Convert ComPtr<> vector to raw pointer array for PSSetShaderResources.
                // Passing ComPtr<> storage directly is undefined and can corrupt bindings.
                std::vector<ID3D11ShaderResourceView*> raw_srvs;
                raw_srvs.reserve(srv_count);
                for (const auto& srv : m_textures[slot])
                {
                    raw_srvs.push_back(srv.Get());
                }

                context->PSSetShaderResources(slot, srv_count, raw_srvs.data());
                for (UINT i = 0; i < srv_count && (slot + static_cast<int>(i)) < 4; ++i)
                {
                    covered_slots[slot + i] = true;
                }
            }
        }

        // Explicitly unbind uncovered slots to avoid stale SRV bindings
        // (e.g., shadow map from previous map render causing validation errors).
        ID3D11ShaderResourceView* nullSRV = nullptr;
        for (int slot = 0; slot < 4; ++slot)
        {
            if (!covered_slots[slot])
            {
                context->PSSetShaderResources(slot, 1, &nullSRV);
            }
        }

        context->DrawIndexed(num_indices, 0, 0);
    }

private:
    Mesh m_mesh;
    int m_mesh_id;
    PerObjectCB m_per_object_data;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer_high; // High LOD
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer_medium; // Medium LOD
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer_low; // Low LOD

    std::array<std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>, 4> m_textures;
};
