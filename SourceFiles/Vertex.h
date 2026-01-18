#pragma once

#include <cstdint>

using namespace DirectX;

struct GWVertex
{
    XMFLOAT3 position;     // The position of the vertex
    XMFLOAT3 normal;       // The normal of the vertex
    XMFLOAT2 tex_coord0;   // First texture coordinate
    XMFLOAT2 tex_coord1;   // Second texture coordinate
    XMFLOAT2 tex_coord2;   // Third texture coordinate
    XMFLOAT2 tex_coord3;   // Fourth texture coordinate
    XMFLOAT2 tex_coord4;   // Fifth texture coordinate
    XMFLOAT2 tex_coord5;   // Sixth texture coordinate
    XMFLOAT2 tex_coord6;   // Seventh texture coordinate
    XMFLOAT2 tex_coord7;   // Eighth texture coordinate
    XMFLOAT3 tangent;      // The tangent of the vertex
    XMFLOAT3 bitangent;    // The tangent of the vertex

	GWVertex()
		: position(XMFLOAT3(0.0f, 0.0f, 0.0f)),
		  normal(XMFLOAT3(0.0f, 0.0f, 0.0f)),
		  tex_coord0(XMFLOAT2(0.0f, 0.0f)),
		  tex_coord1(XMFLOAT2(0.0f, 0.0f)),
		  tex_coord2(XMFLOAT2(0.0f, 0.0f)),
		  tex_coord3(XMFLOAT2(0.0f, 0.0f)),
		  tex_coord4(XMFLOAT2(0.0f, 0.0f)),
		  tex_coord5(XMFLOAT2(0.0f, 0.0f)),
		  tex_coord6(XMFLOAT2(0.0f, 0.0f)),
		  tex_coord7(XMFLOAT2(0.0f, 0.0f)),
		  tangent(XMFLOAT3(0.0f, 0.0f, 0.0f)),
		  bitangent(XMFLOAT3(0.0f, 0.0f, 0.0f)) { }

	GWVertex(XMFLOAT3 p, XMFLOAT3 n, XMFLOAT2 tc0)
		: position(p), normal(n), tex_coord0(tc0) {}

	GWVertex(XMFLOAT3 p, XMFLOAT3 n, XMFLOAT2 tc0, XMFLOAT2 tc1, XMFLOAT2 tc2, XMFLOAT2 tc3, XMFLOAT2 tc4, XMFLOAT2 tc5,
	         XMFLOAT2 tc6, XMFLOAT2 tc7, XMFLOAT3 tan, XMFLOAT3 bitan)
		: position(p), normal(n), tex_coord0(tc0), tex_coord1(tc1), tex_coord2(tc2), tex_coord3(tc3), tex_coord4(tc4),
		  tex_coord5(tc5), tex_coord6(tc6), tex_coord7(tc7), tangent(tan), bitangent(bitan) {}
};

/**
 * @brief Extended vertex type with bone weights for skeletal animation.
 *
 * Supports up to 4 bone influences per vertex (linear blend skinning).
 * Bone indices reference the skeleton bone array via the bone group palette system.
 * Weights must sum to 1.0 for correct skinning.
 */
struct SkinnedGWVertex : public GWVertex
{
    uint32_t boneIndices[4];  // Indices into skeleton bone array (up to 4 bones)
    float boneWeights[4];     // Corresponding weights (must sum to 1.0)

    SkinnedGWVertex()
        : GWVertex()
    {
        boneIndices[0] = boneIndices[1] = boneIndices[2] = boneIndices[3] = 0;
        boneWeights[0] = 1.0f;  // Default: fully influenced by bone 0
        boneWeights[1] = boneWeights[2] = boneWeights[3] = 0.0f;
    }

    SkinnedGWVertex(const GWVertex& base)
        : GWVertex(base)
    {
        boneIndices[0] = boneIndices[1] = boneIndices[2] = boneIndices[3] = 0;
        boneWeights[0] = 1.0f;
        boneWeights[1] = boneWeights[2] = boneWeights[3] = 0.0f;
    }

    /**
     * @brief Sets single bone influence (common case for GW models).
     */
    void SetSingleBone(uint32_t boneIndex)
    {
        boneIndices[0] = boneIndex;
        boneIndices[1] = boneIndices[2] = boneIndices[3] = 0;
        boneWeights[0] = 1.0f;
        boneWeights[1] = boneWeights[2] = boneWeights[3] = 0.0f;
    }

    /**
     * @brief Sets multiple bone influences.
     * @param indices Array of bone indices (must have 'count' elements)
     * @param weights Array of weights (must have 'count' elements, sum to 1.0)
     * @param count Number of influences (1-4)
     */
    void SetBoneInfluences(const uint32_t* indices, const float* weights, int count)
    {
        for (int i = 0; i < 4; i++)
        {
            if (i < count)
            {
                boneIndices[i] = indices[i];
                boneWeights[i] = weights[i];
            }
            else
            {
                boneIndices[i] = 0;
                boneWeights[i] = 0.0f;
            }
        }
    }

    /**
     * @brief Normalizes bone weights to sum to 1.0.
     */
    void NormalizeWeights()
    {
        float sum = boneWeights[0] + boneWeights[1] + boneWeights[2] + boneWeights[3];
        if (sum > 0.0001f)
        {
            float invSum = 1.0f / sum;
            boneWeights[0] *= invSum;
            boneWeights[1] *= invSum;
            boneWeights[2] *= invSum;
            boneWeights[3] *= invSum;
        }
        else
        {
            // No weights, default to bone 0
            boneWeights[0] = 1.0f;
            boneWeights[1] = boneWeights[2] = boneWeights[3] = 0.0f;
        }
    }
};

// Define the input layout
// Define the input layout
inline extern D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GWVertex, position), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GWVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GWVertex, tex_coord0), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GWVertex, tex_coord1), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GWVertex, tex_coord2), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GWVertex, tex_coord3), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GWVertex, tex_coord4), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"TEXCOORD", 5, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GWVertex, tex_coord5), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"TEXCOORD", 6, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GWVertex, tex_coord6), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GWVertex, tex_coord7), D3D11_INPUT_PER_VERTEX_DATA,
   0},
  {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GWVertex, tangent), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TANGENT", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GWVertex, bitangent), D3D11_INPUT_PER_VERTEX_DATA, 0}
};

// Input layout for skinned meshes (includes bone indices and weights)
inline extern D3D11_INPUT_ELEMENT_DESC skinnedInputLayoutDesc[] = {
  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SkinnedGWVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SkinnedGWVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinnedGWVertex, tex_coord0), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinnedGWVertex, tex_coord1), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinnedGWVertex, tex_coord2), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinnedGWVertex, tex_coord3), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinnedGWVertex, tex_coord4), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 5, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinnedGWVertex, tex_coord5), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 6, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinnedGWVertex, tex_coord6), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinnedGWVertex, tex_coord7), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SkinnedGWVertex, tangent), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TANGENT", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SkinnedGWVertex, bitangent), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, offsetof(SkinnedGWVertex, boneIndices), D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SkinnedGWVertex, boneWeights), D3D11_INPUT_PER_VERTEX_DATA, 0}
};

inline int get_most_significant_bit_pos(uint32_t value)
{
    int bit_position = 31; // For 32-bit unsigned integer, highest bit position is 31
    if (value != 0)
    {
        while ((value >> bit_position) == 0)
        {
            bit_position--;
        }
    }
    return bit_position;
}

inline uint32_t FVF_to_ActualFVF(uint32_t FVF)
{
    int most_significant_bit_index;
    uint32_t ActualFVF;
    uint32_t currFlag;

    uint32_t FVF_to_ActualFVF_converter_array[] = {
      0x00000003, 0x00008006, 0x00000001, 0x00000002, 0x00000004, 0x00000010, 0x00000010, 0x00010000,
      0x00000020, 0x00020000, 0x00000008, 0x00000040, 0x00000040, 0x00040000, 0x00000000, 0x00000000};

    ActualFVF = 0;
    assert((FVF & 0x40) == 0 || (FVF & 0x34) == 0);

    if ((FVF & 0xff00) != 0)
    {
        most_significant_bit_index = get_most_significant_bit_pos(FVF & 0xff00);
        ActualFVF = (most_significant_bit_index - 7) << 8;
        FVF = FVF & 0xffff00ff;
    }
    if (FVF != 0)
    {
        const uint32_t* pArray = &FVF_to_ActualFVF_converter_array[1];
        do
        {
            currFlag = *(pArray - 1);
            if (currFlag == 0)
            {
                return 0;
            }

            if ((currFlag & FVF) == currFlag)
            {
                FVF = FVF & ~currFlag;
                ActualFVF = ActualFVF | *pArray;
            }
            pArray = pArray + 2;
        } while (FVF != 0);
    }
    return ActualFVF;
}

//The below defines are taken from: d3d9types.h
// Flexible vertex format bits
#define D3DFVF_RESERVED0 0x001
#define D3DFVF_POSITION_MASK 0x400E
#define D3DFVF_XYZ 0x002
#define D3DFVF_XYZRHW 0x004
#define D3DFVF_XYZB1 0x006
#define D3DFVF_XYZB2 0x008
#define D3DFVF_XYZB3 0x00a
#define D3DFVF_XYZB4 0x00c
#define D3DFVF_XYZB5 0x00e
#define D3DFVF_XYZW 0x4002

#define D3DFVF_NORMAL 0x010
#define D3DFVF_PSIZE 0x020
#define D3DFVF_DIFFUSE 0x040
#define D3DFVF_SPECULAR 0x080

#define D3DFVF_TEXCOUNT_MASK 0xf00
#define D3DFVF_TEXCOUNT_SHIFT 8
#define D3DFVF_TEX0 0x000
#define D3DFVF_TEX1 0x100
#define D3DFVF_TEX2 0x200
#define D3DFVF_TEX3 0x300
#define D3DFVF_TEX4 0x400
#define D3DFVF_TEX5 0x500
#define D3DFVF_TEX6 0x600
#define D3DFVF_TEX7 0x700
#define D3DFVF_TEX8 0x800

#define D3DFVF_LASTBETA_UBYTE4 0x1000
#define D3DFVF_LASTBETA_D3DCOLOR 0x8000

#define D3DFVF_RESERVED2 0x6000 // 2 reserved bits`
