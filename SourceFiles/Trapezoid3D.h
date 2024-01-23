#pragma once
#include "MeshInstance.h"
#include "DXMathHelpers.h"

class Trapezoid3D : public MeshInstance
{
public:
    Trapezoid3D(ID3D11Device* device, const Vertex3& TL, const Vertex3& TR,
        const Vertex3& BL, const Vertex3& BR, float height, int id)
        : MeshInstance(device, GenerateTrapezoidMesh(TL, TR, BL, BR, height), id)
    {
    }

private:
    Mesh GenerateTrapezoidMesh(const Vertex3& TL, const Vertex3& TR,
        const Vertex3& BL, const Vertex3& BR, float height)
    {
        // Convert Vertex3 to XMFLOAT3
        XMFLOAT3 xTL = Vertex3ToXMFLOAT3(TL);
        XMFLOAT3 xTR = Vertex3ToXMFLOAT3(TR);
        XMFLOAT3 xBL = Vertex3ToXMFLOAT3(BL);
        XMFLOAT3 xBR = Vertex3ToXMFLOAT3(BR);

        // Calculate vertices for the top face of the trapezoid
        XMFLOAT3 TLtop(xTL.x, xTL.y + height, xTL.z);
        XMFLOAT3 TRtop(xTR.x, xTR.y + height, xTR.z);
        XMFLOAT3 BLtop(xBL.x, xBL.y + height, xBL.z);
        XMFLOAT3 BRtop(xBR.x, xBR.y + height, xBR.z);

        // Define texture coordinates (assuming simple mapping)
        XMFLOAT2 tex00(0.0f, 0.0f);
        XMFLOAT2 tex01(0.0f, 1.0f);
        XMFLOAT2 tex10(1.0f, 0.0f);
        XMFLOAT2 tex11(1.0f, 1.0f);

        // Calculate normals for each face
        XMFLOAT3 normalBottom = compute_normal(xBL, xBR, xTR);
        XMFLOAT3 normalTop = compute_normal(BLtop, BRtop, TRtop);
        XMFLOAT3 normalSideTLBL = compute_normal(xTL, TLtop, BLtop);
        XMFLOAT3 normalSideBLBR = compute_normal(xBL, BLtop, BRtop);
        XMFLOAT3 normalSideBRTL = compute_normal(xBR, BRtop, TRtop);
        XMFLOAT3 normalSideTRTL = compute_normal(xTR, TRtop, TLtop);

        // Define vertices for each face
        std::vector<GWVertex> vertices = {
            // Bottom face (trapezoid)
            {xTL, normalBottom, tex00}, {xTR, normalBottom, tex10}, {xBR, normalBottom, tex11}, {xBL, normalBottom, tex01},
            // Top face (trapezoid)
            {TLtop, normalTop, tex00}, {TRtop, normalTop, tex10}, {BRtop, normalTop, tex11}, {BLtop, normalTop, tex01},
            // Side faces (rectangles)
            // Side TL-BL
            {xTL, normalSideTLBL, tex00}, {TLtop, normalSideTLBL, tex01}, {BLtop, normalSideTLBL, tex11}, {xBL, normalSideTLBL, tex10},
            // Side BL-BR
            {xBL, normalSideBLBR, tex00}, {BLtop, normalSideBLBR, tex01}, {BRtop, normalSideBLBR, tex11}, {xBR, normalSideBLBR, tex10},
            // Side BR-TR
            {xBR, normalSideBRTL, tex00}, {BRtop, normalSideBRTL, tex01}, {TRtop, normalSideBRTL, tex11}, {xTR, normalSideBRTL, tex10},
            // Side TR-TL
            {xTR, normalSideTRTL, tex00}, {TRtop, normalSideTRTL, tex01}, {TLtop, normalSideTRTL, tex11}, {xTL, normalSideTRTL, tex10}
        };

        // Define indices for each face
        std::vector<uint32_t> indices = {
            // Indices for the bottom face
            0, 1, 2, 0, 2, 3,
            // Indices for the top face
            4, 5, 6, 4, 6, 7,
            // Indices for the side faces
            8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23
        };

        return Mesh(vertices, indices);
    }

    std::unique_ptr<MeshInstance> m_meshInstance;
};
