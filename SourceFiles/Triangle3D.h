#pragma once
#include "MeshInstance.h"
#include <DXMathHelpers.h>

class Triangle3D : public MeshInstance
{
public:
    Triangle3D(ID3D11Device* device, const Vertex3& v1, const Vertex3& v2, const Vertex3& v3, float height, int id)
        : MeshInstance(device, GenerateTriangleMesh(v1, v2, v3, height), id)
    {
    }

private:
    Mesh GenerateTriangleMesh(const Vertex3& v1, const Vertex3& v2, const Vertex3& v3, float height)
    {
        // Convert Vertex3 to XMFLOAT3
        XMFLOAT3 xV1 = Vertex3ToXMFLOAT3(v1);
        XMFLOAT3 xV2 = Vertex3ToXMFLOAT3(v2);
        XMFLOAT3 xV3 = Vertex3ToXMFLOAT3(v3);

        // Calculate vertices for the top face of the triangle using height
        XMFLOAT3 V1top(xV1.x, xV1.y, xV1.z + height);
        XMFLOAT3 V2top(xV2.x, xV2.y, xV2.z + height);
        XMFLOAT3 V3top(xV3.x, xV3.y, xV3.z + height);

        // Define texture coordinates (assuming simple mapping)
        XMFLOAT2 tex00(0.0f, 0.0f);
        XMFLOAT2 tex01(0.0f, 1.0f);
        XMFLOAT2 tex10(1.0f, 0.0f);
        XMFLOAT2 tex11(1.0f, 1.0f);

        // Calculate normals for each face
        XMFLOAT3 normalBottom = compute_normal(xV1, xV2, xV3);
        XMFLOAT3 normalTop = compute_normal(V1top, V2top, V3top);
        XMFLOAT3 normalSide1 = compute_normal(xV1, V1top, V2top);
        XMFLOAT3 normalSide2 = compute_normal(xV2, V2top, V3top);
        XMFLOAT3 normalSide3 = compute_normal(xV3, V3top, V1top);

        // Define vertices for each face
        std::vector<GWVertex> vertices = {
            // Bottom face (triangle)
            {xV1, normalBottom, tex00}, {xV2, normalBottom, tex10}, {xV3, normalBottom, tex11},
            // Top face (triangle)
            {V1top, normalTop, tex00}, {V2top, normalTop, tex10}, {V3top, normalTop, tex11},
            // Side faces (rectangles)
            {xV1, normalSide1, tex00}, {V1top, normalSide1, tex01}, {V2top, normalSide1, tex11}, {xV2, normalSide1, tex10},
            {xV2, normalSide2, tex00}, {V2top, normalSide2, tex01}, {V3top, normalSide2, tex11}, {xV3, normalSide2, tex10},
            {xV3, normalSide3, tex00}, {V3top, normalSide3, tex01}, {V1top, normalSide3, tex11}, {xV1, normalSide3, tex10}
        };

        // Define indices for each face
        std::vector<uint32_t> indices = {
            // Indices for the bottom face
            0, 1, 2,
            // Indices for the top face
            3, 4, 5,
            // Indices for the side faces
            6, 7, 8, 6, 8, 9, 10, 11, 12, 10, 12, 13, 14, 15, 16, 14, 16, 17
        };

        return Mesh(vertices, indices);
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};