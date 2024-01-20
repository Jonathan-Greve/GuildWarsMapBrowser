#pragma once
#include "MeshInstance.h"

class Dome : public MeshInstance
{
public:
    Dome(ID3D11Device* device, float radius, uint32_t sliceCount, uint32_t stackCount, int id)
        : MeshInstance(device, GenerateDomeMesh(radius, sliceCount, stackCount), id)
    {
    }

    static Mesh GenerateDomeMesh(float radius, uint32_t numSlices, uint32_t numStacks)
    {
        std::vector<GWVertex> vertices;
        std::vector<uint32_t> indices;

        // Generate vertices
        // Top vertex
        vertices.emplace_back(
            DirectX::XMFLOAT3(0.0f, radius, 0.0f), // Position
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),  // Normal
            DirectX::XMFLOAT2(0.0f, 0.0f));       // TexCoords

        // Calculate the stacks half the way through the sphere for the dome
        float phiStep = DirectX::XM_PI / numStacks;
        float thetaStep = 2.0f * DirectX::XM_PI / numSlices;

        // Calculate vertices for the dome (top hemisphere)
        for (uint32_t i = 1; i <= numStacks / 2; ++i)
        {
            float phi = i * phiStep;

            // vertices of stack ring
            for (uint32_t j = 0; j <= numSlices; ++j)
            {
                float theta = j * thetaStep;

                DirectX::XMFLOAT3 position(
                    radius * sinf(phi) * cosf(theta),
                    radius * cosf(phi),
                    radius * sinf(phi) * sinf(theta));

                DirectX::XMFLOAT3 normal = position;
                DirectX::XMVECTOR normalVector = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&normal));
                DirectX::XMStoreFloat3(&normal, normalVector);

                float u = theta / (2.0f * DirectX::XM_PI);
                float v = phi / DirectX::XM_PI;

                vertices.emplace_back(position, normal, DirectX::XMFLOAT2(u, v));
            }
        }

        // Indices for the top cap
        for (uint32_t i = 1; i <= numSlices; ++i)
        {
            indices.push_back(0);
            indices.push_back(i + 1);
            indices.push_back(i);
        }

        // Indices for the middle stacks
        uint32_t baseIndex = 1;
        uint32_t ringVertexCount = numSlices + 1;
        for (uint32_t i = 0; i < numStacks / 2 - 1; ++i)
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

        return Mesh(vertices, indices);
    }
};
