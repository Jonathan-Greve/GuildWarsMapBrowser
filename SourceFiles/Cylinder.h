#pragma once
#include "MeshInstance.h"

class Cylinder : public MeshInstance
{
public:
    Cylinder(ID3D11Device* device, float radius, float height, uint32_t sliceCount, uint32_t stackCount, int id)
        : MeshInstance(device, GenerateCylinderMesh(radius, height, sliceCount, stackCount), id)
    {
    }

private:
    Mesh GenerateCylinderMesh(float radius, float height, uint32_t numSlices, uint32_t numStacks)
    {
        std::vector<GWVertex> vertices;
        std::vector<uint32_t> indices;

        // Top center vertex
        vertices.emplace_back(
            DirectX::XMFLOAT3(0.0f, height * 0.5f, 0.0f), // Position
            DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),          // Normal
            DirectX::XMFLOAT2(0.0f, 0.0f));               // TexCoords

        float thetaStep = 2.0f * DirectX::XM_PI / numSlices;
        float stackHeight = height / numStacks;
        float uStep = 1.0f / numSlices;
        float vStep = 0.75f / numStacks;

        // Top lid vertices
        for (uint32_t i = 0; i <= numSlices; ++i)
        {
            float theta = i * thetaStep;
            DirectX::XMFLOAT3 position(
                radius * cosf(theta),
                height * 0.5f,
                radius * sinf(theta));

            float u = (i < numSlices) ? uStep * i : 1.0f; // Adjust U for seam
            float v = 0.25f;

            vertices.emplace_back(position, DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(u, v));
        }

        // Cylinder body vertices
        for (uint32_t j = 0; j <= numStacks; ++j)
        {
            float y = height * 0.5f - j * stackHeight;

            for (uint32_t i = 0; i <= numSlices; ++i)
            {
                float theta = i * thetaStep;
                DirectX::XMFLOAT3 position(
                    radius * cosf(theta),
                    y,
                    radius * sinf(theta));

                DirectX::XMFLOAT3 normal(
                    cosf(theta),
                    0.0f,
                    sinf(theta));

                float u = (i < numSlices) ? uStep * i : 1.0f; // Adjust U for the seam
                float v = 0.25f + j * vStep;

                vertices.emplace_back(position, normal, DirectX::XMFLOAT2(u, v));
            }
        }


        // Indices for the top lid
        uint32_t ringVertexCount = numSlices + 1;
        for (uint32_t i = 1; i <= numSlices; ++i)
        {
            indices.push_back(0);
            indices.push_back(i);
            indices.push_back((i % ringVertexCount) + 1);  // Wrap around
        }

        // Indices for the body
        uint32_t baseIndex = 1 + ringVertexCount;  // Skip the top center and lid vertices

        for (uint32_t j = 0; j < numStacks; ++j)
        {
            for (uint32_t i = 0; i < numSlices; ++i)
            {
                indices.push_back(baseIndex + j * ringVertexCount + i);
                indices.push_back(baseIndex + (j + 1) * ringVertexCount + i);
                indices.push_back(baseIndex + j * ringVertexCount + (i + 1) % ringVertexCount);

                indices.push_back(baseIndex + j * ringVertexCount + (i + 1) % ringVertexCount);
                indices.push_back(baseIndex + (j + 1) * ringVertexCount + i);
                indices.push_back(baseIndex + (j + 1) * ringVertexCount + (i + 1) % ringVertexCount);
            }
        }

        return Mesh(vertices, indices);
    }
};