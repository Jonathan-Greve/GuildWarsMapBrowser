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
        // Generate sphere mesh here
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
