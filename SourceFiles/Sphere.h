#pragma once
#include "MeshInstance.h"

class Sphere : public MeshInstance
{
public:
    Sphere(ID3D11Device* device, float radius, uint32_t sliceCount, uint32_t stackCount, int id)
        : MeshInstance(device, GenerateSphereMesh(radius, sliceCount, stackCount), id)
    {
    }

private:
    Mesh GenerateSphereMesh(float radius, uint32_t numSlices, uint32_t numStacks)
    {
        std::vector<GWVertex> vertices;
        std::vector<uint32_t> indices;

        // Generate vertices
        vertices.emplace_back(DirectX::XMFLOAT3(0.0f, radius, 0.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
                              XMFLOAT2(0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f),
                              XMFLOAT2(0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f),
                              XMFLOAT2(0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f));

        float phiStep = DirectX::XM_PI / numStacks;
        float thetaStep = 2.0f * DirectX::XM_PI / numSlices;

        for (uint32_t i = 1; i <= numStacks - 1; ++i)
        {
            float phi = i * phiStep;
            for (uint32_t j = 0; j <= numSlices; ++j)
            {
                float theta = j * thetaStep;

                DirectX::XMFLOAT3 position(radius * sinf(phi) * cosf(theta), radius * cosf(phi),
                                           radius * sinf(phi) * sinf(theta));

                DirectX::XMFLOAT3 normal = position;
                DirectX::XMVECTOR normalVector = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&normal));
                DirectX::XMStoreFloat3(&normal, normalVector);

                float u = theta / (2.0f * DirectX::XM_PI);
                float v = phi / DirectX::XM_PI;

                vertices.emplace_back(position, normal, DirectX::XMFLOAT2(u, v));
            }
        }

        vertices.emplace_back(DirectX::XMFLOAT3(0.0f, -radius, 0.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f),
                              DirectX::XMFLOAT2(0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f),
                              XMFLOAT2(0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f),
                              XMFLOAT2(0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f));

        // Generate indices
        for (uint32_t i = 1; i <= numSlices; ++i)
        {
            indices.push_back(0);
            indices.push_back(i + 1);
            indices.push_back(i);
        }

        uint32_t baseIndex = 1;
        uint32_t ringVertexCount = numSlices + 1;
        for (uint32_t i = 0; i < numStacks - 2; ++i)
        {
            for (uint32_t j = 0; j < numSlices; ++j)
            {
                indices.push_back(baseIndex + i * ringVertexCount + j);
                indices.push_back(baseIndex + i * ringVertexCount + j + 1);
                indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

                indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
                indices.push_back(baseIndex + i * ringVertexCount + j + 1);
                indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
            }
        }

        // Generate the indices for the bottom cap
        uint32_t southPoleIndex = (uint32_t)vertices.size() - 1;
        baseIndex = southPoleIndex - ringVertexCount;

        for (uint32_t i = 0; i < numSlices; ++i)
        {
            indices.push_back(southPoleIndex);
            indices.push_back(baseIndex + i);
            indices.push_back(baseIndex + i + 1);
        }

        return Mesh(vertices, indices);
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
