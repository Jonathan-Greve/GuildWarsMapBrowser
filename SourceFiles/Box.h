#pragma once
#include "MeshInstance.h"

class Box : public MeshInstance
{
public:
    Box(ID3D11Device* device, const DirectX::XMFLOAT3& size, int id)
        : MeshInstance(device, GenerateBoxMesh(size), id)
    {
    }

private:
    Mesh GenerateBoxMesh(const DirectX::XMFLOAT3& size)
    {
        float half_width = size.x * 0.5f;
        float half_height = size.y * 0.5f;
        float half_depth = size.z * 0.5f;

        // Define the 8 corner points of the box.
        DirectX::XMFLOAT3 topLeftFront(-half_width, half_height, -half_depth);
        DirectX::XMFLOAT3 topLeftBack(-half_width, half_height, half_depth);
        DirectX::XMFLOAT3 topRightFront(half_width, half_height, -half_depth);
        DirectX::XMFLOAT3 topRightBack(half_width, half_height, half_depth);
        DirectX::XMFLOAT3 bottomLeftFront(-half_width, -half_height, -half_depth);
        DirectX::XMFLOAT3 bottomLeftBack(-half_width, -half_height, half_depth);
        DirectX::XMFLOAT3 bottomRightFront(half_width, -half_height, -half_depth);
        DirectX::XMFLOAT3 bottomRightBack(half_width, -half_height, half_depth);

        // Define the normals for each face of the box.
        DirectX::XMFLOAT3 frontNormal(0.0f, 0.0f, -1.0f);
        DirectX::XMFLOAT3 backNormal(0.0f, 0.0f, 1.0f);
        DirectX::XMFLOAT3 leftNormal(-1.0f, 0.0f, 0.0f);
        DirectX::XMFLOAT3 rightNormal(1.0f, 0.0f, 0.0f);
        DirectX::XMFLOAT3 topNormal(0.0f, 1.0f, 0.0f);
        DirectX::XMFLOAT3 bottomNormal(0.0f, -1.0f, 0.0f);

        // Define the texture coordinates for each vertex.
        DirectX::XMFLOAT2 tex00(0.0f, 0.0f);
        DirectX::XMFLOAT2 tex01(0.0f, 1.0f);
        DirectX::XMFLOAT2 tex10(1.0f, 0.0f);
        DirectX::XMFLOAT2 tex11(1.0f, 1.0f);

        std::vector<GWVertex> vertices = {
          // Front face
          {topLeftFront, frontNormal, tex11, tex11, tex11, tex11, tex11, tex11, tex11, tex11},
          {bottomLeftFront, frontNormal, tex01, tex01, tex01, tex01, tex01, tex01, tex01, tex01},
          {bottomRightFront, frontNormal, tex00, tex00, tex00, tex00, tex00, tex00, tex00, tex00},
          {topRightFront, frontNormal, tex10, tex10, tex10, tex10, tex10, tex10, tex10, tex10},
          // Back face
          {topRightBack, backNormal, tex11, tex11, tex11, tex11, tex11, tex11, tex11, tex11},
          {bottomRightBack, backNormal, tex01, tex01, tex01, tex01, tex01, tex01, tex01, tex01},
          {bottomLeftBack, backNormal, tex00, tex00, tex00, tex00, tex00, tex00, tex00, tex00},
          {topLeftBack, backNormal, tex10, tex10, tex10, tex10, tex10, tex10, tex10, tex10},
          // Left face
          {topLeftBack, leftNormal, tex11, tex11, tex11, tex11, tex11, tex11, tex11, tex11},
          {bottomLeftBack, leftNormal, tex01, tex01, tex01, tex01, tex01, tex01, tex01, tex01},
          {bottomLeftFront, leftNormal, tex00, tex00, tex00, tex00, tex00, tex00, tex00, tex00},
          {topLeftFront, leftNormal, tex10, tex10, tex10, tex10, tex10, tex10, tex10, tex10},
          // Right face
          {topRightFront, rightNormal, tex11, tex11, tex11, tex11, tex11, tex11, tex11, tex11},
          {bottomRightFront, rightNormal, tex01, tex01, tex01, tex01, tex01, tex01, tex01, tex01},
          {bottomRightBack, rightNormal, tex00, tex00, tex00, tex00, tex00, tex00, tex00, tex00},
          {topRightBack, rightNormal, tex10, tex10, tex10, tex10, tex10, tex10, tex10, tex10},
          // Top face
          {topLeftBack, topNormal, tex11, tex11, tex11, tex11, tex11, tex11, tex11, tex11},
          {topLeftFront, topNormal, tex01, tex01, tex01, tex01, tex01, tex01, tex01, tex01},
          {topRightFront, topNormal, tex00, tex00, tex00, tex00, tex00, tex00, tex00, tex00},
          {topRightBack, topNormal, tex10, tex10, tex10, tex10, tex10, tex10, tex10, tex10},
          {bottomLeftFront, bottomNormal, tex11, tex11, tex11, tex11, tex11, tex11, tex11, tex11},
          {bottomLeftBack, bottomNormal, tex01, tex01, tex01, tex01, tex01, tex01, tex01, tex01},
          {bottomRightBack, bottomNormal, tex00, tex00, tex00, tex00, tex00, tex00, tex00, tex00},
          {bottomRightFront, bottomNormal, tex10, tex10, tex10, tex10, tex10, tex10, tex10, tex10},
        };

        std::vector<uint32_t> indices = {// Front face
                                         1, 0, 3, 1, 3, 2,
                                         // Back face
                                         5, 4, 7, 5, 7, 6,
                                         // Left face
                                         9, 8, 11, 9, 11, 10,
                                         // Right face
                                         13, 12, 15, 13, 15, 14,
                                         // Top face
                                         17, 16, 19, 17, 19, 18,
                                         // Bottom face
                                         21, 20, 23, 21, 23, 22};

        return Mesh(vertices, indices);
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
