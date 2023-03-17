#pragma once
#include <unordered_map>
#include <memory>
#include <DirectXMath.h>
#include "RenderCommand.h"
#include "RenderBatch.h"
#include "Box.h"
#include "Sphere.h"
#include "Line.h"
#include "RenderConstants.h"

class MeshManager
{
public:
    MeshManager(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
        : m_device(device)
        , m_deviceContext(deviceContext)
        , m_nextMeshID(0)
        , m_needsUpdate(false)
    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.ByteWidth = sizeof(PerObjectCB);
        cbDesc.StructureByteStride = sizeof(PerObjectCB);
        m_device->CreateBuffer(&cbDesc, nullptr, &m_perObjectCB);

        // Set the constant buffer for the vertex shader
        ID3D11Buffer* constantBuffers[] = {m_perObjectCB.Get()};
        m_deviceContext->VSSetConstantBuffers(PER_OBJECT_CB_SLOT, 1, constantBuffers);
        m_deviceContext->PSSetConstantBuffers(PER_OBJECT_CB_SLOT, 1, constantBuffers);
    }

    int AddBox(const DirectX::XMFLOAT3& size)
    {
        int meshID = m_nextMeshID++;
        m_triangleMeshes[meshID] = std::make_shared<Box>(m_device, size, meshID);
        m_needsUpdate = true;
        return meshID;
    }

    int AddSphere(float radius, uint32_t numSlices, uint32_t numStacks)
    {
        int meshID = m_nextMeshID++;
        m_triangleMeshes[meshID] = std::make_shared<Sphere>(m_device, radius, numSlices, numStacks, meshID);
        m_needsUpdate = true;
        return meshID;
    }

    int AddLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
    {
        int meshID = m_nextMeshID++;
        m_lineMeshes[meshID] = std::make_shared<Line>(m_device, start, end, meshID);
        m_needsUpdate = true;
        return meshID;
    }

    int AddCustomMesh(const Mesh& mesh)
    {
        int meshID = m_nextMeshID++;
        m_triangleMeshes[meshID] = std::make_shared<MeshInstance>(m_device, mesh, meshID);
        m_needsUpdate = true;
        return meshID;
    }

    bool RemoveMesh(int meshID)
    {
        auto it = m_triangleMeshes.find(meshID);
        if (it != m_triangleMeshes.end())
        {
            m_triangleMeshes.erase(it);
            m_needsUpdate = true;
            return true;
        }

        it = m_lineMeshes.find(meshID);
        if (it != m_lineMeshes.end())
        {
            m_lineMeshes.erase(it);
            m_needsUpdate = true;
            return true;
        }

        return false;
    }

    void AddTextureToMesh(int meshID, ID3D11ShaderResourceView* texture)
    {
        auto it = m_triangleMeshes.find(meshID);
        if (it != m_triangleMeshes.end())
        {
            it->second->SetTexture(texture);
        }
    }

    void UpdateMeshPerObjectData(int meshID, const PerObjectCB& data)
    {
        auto it = m_triangleMeshes.find(meshID);
        if (it != m_triangleMeshes.end())
        {
            it->second->SetPerObjectData(data);
            return;
        }

        it = m_lineMeshes.find(meshID);
        if (it != m_lineMeshes.end())
        {
            it->second->SetPerObjectData(data);
            return;
        }
    }

    void Update(float dt)
    {
        if (m_needsUpdate)
        {
            m_renderBatch.Clear();
            for (auto& [id, mesh] : m_triangleMeshes)
            {
                RenderCommand command = {mesh, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST};
                m_renderBatch.AddCommand(command);
            }
            for (auto& [id, mesh] : m_lineMeshes)
            {
                RenderCommand command = {mesh, D3D11_PRIMITIVE_TOPOLOGY_LINELIST};
                m_renderBatch.AddCommand(command);
            }
            m_renderBatch.SortCommands();
            m_needsUpdate = false;
        }
    }

    void Render()
    {
        D3D11_PRIMITIVE_TOPOLOGY currentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        for (const RenderCommand& command : m_renderBatch.GetCommands())
        {
            if (command.primitiveTopology != currentTopology)
            {
                m_deviceContext->IASetPrimitiveTopology(command.primitiveTopology);
                currentTopology = command.primitiveTopology;
            }

            PerObjectCB transposedData = command.meshInstance->GetPerObjectData();

            // Load World matrix into an XMMATRIX, transpose it, and store it back into an XMFLOAT4X4
            DirectX::XMMATRIX worldMatrix = DirectX::XMLoadFloat4x4(&transposedData.world);
            worldMatrix = DirectX::XMMatrixTranspose(worldMatrix);
            DirectX::XMStoreFloat4x4(&transposedData.world, worldMatrix);

            // Update the shared constant buffer
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            m_deviceContext->Map(m_perObjectCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            memcpy(mappedResource.pData, &transposedData, sizeof(PerObjectCB));
            m_deviceContext->Unmap(m_perObjectCB.Get(), 0);

            command.meshInstance->Draw(m_deviceContext);
        }
    }

private:
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    int m_nextMeshID;
    bool m_needsUpdate;
    std::unordered_map<int, std::shared_ptr<MeshInstance>> m_triangleMeshes;
    std::unordered_map<int, std::shared_ptr<MeshInstance>> m_lineMeshes;
    RenderBatch m_renderBatch;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_perObjectCB;
};
