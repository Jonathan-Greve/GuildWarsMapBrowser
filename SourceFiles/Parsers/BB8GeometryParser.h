#pragma once

#include "../Vertex.h"
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <optional>
#include <unordered_map>

using namespace DirectX;

namespace GW::Parsers {

// BB8/FA0 chunk IDs
constexpr uint32_t CHUNK_ID_BB8 = 0x00000BB8;  // Geometry data (type 2 "other" format)
constexpr uint32_t CHUNK_ID_FA0 = 0x00000FA0;  // Geometry data (type 2 "standard" format)

// FVF (Flexible Vertex Format) tables for calculating vertex sizes
// Position format sizes (indexed by low 4 bits of FVF)
constexpr uint32_t FVF_POSITION_SIZE[16] = {
    0, 12, 4, 16, 12, 24, 16, 28, 12, 16, 20, 24, 28, 32, 36, 40
};

// Normal format sizes (indexed by bits 4-6 of FVF)
constexpr uint32_t FVF_NORMAL_SIZE[8] = {
    0, 12, 4, 4, 8, 0, 0, 0
};

// Texture coordinate format sizes (indexed by bits 8-11 and 12-15 of FVF)
constexpr uint32_t FVF_TEXCOORD_SIZE[16] = {
    0, 8, 8, 16, 8, 16, 16, 24, 8, 16, 16, 24, 16, 24, 24, 32
};

/**
 * @brief Bone group data for skinned mesh rendering.
 *
 * Vertices store a bone GROUP index (0 to num_groups-1), not direct bone IDs.
 * This structure maps bone groups to skeleton bone indices.
 */
struct BoneGroupData
{
    std::vector<uint32_t> groupSizes;           // Number of bones in each group
    std::vector<uint32_t> skeletonBoneIndices;  // Flat array of skeleton bone IDs
    std::vector<uint32_t> groupToFirstBone;     // First skeleton bone for each group (for single-bone skinning)

    /**
     * @brief Builds the group to first bone mapping.
     */
    void BuildGroupMapping()
    {
        groupToFirstBone.clear();
        size_t skelIdx = 0;

        for (uint32_t groupSize : groupSizes)
        {
            if (skelIdx < skeletonBoneIndices.size())
            {
                groupToFirstBone.push_back(skeletonBoneIndices[skelIdx]);
            }
            else
            {
                groupToFirstBone.push_back(0);
            }
            skelIdx += groupSize;
        }
    }

    /**
     * @brief Maps a vertex's bone group index to its skeleton bone.
     *
     * @param groupIndex Bone group index from vertex data.
     * @return Skeleton bone index.
     */
    uint32_t MapGroupToSkeletonBone(uint32_t groupIndex) const
    {
        if (groupIndex < groupToFirstBone.size())
        {
            return groupToFirstBone[groupIndex];
        }
        return 0;
    }

    /**
     * @brief Gets all skeleton bones used by this mesh.
     */
    std::vector<uint32_t> GetAllUsedBones() const
    {
        return skeletonBoneIndices;
    }
};

/**
 * @brief Parsed submesh data including skinning information.
 */
struct ParsedSubmesh
{
    std::vector<SkinnedGWVertex> vertices;
    std::vector<uint32_t> indices;
    BoneGroupData boneGroups;

    // Material/texture information
    uint32_t materialIndex = 0;
    std::vector<uint8_t> textureIndices;

    // Bounding box
    XMFLOAT3 minBounds = {FLT_MAX, FLT_MAX, FLT_MAX};
    XMFLOAT3 maxBounds = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    XMFLOAT3 center = {0, 0, 0};

    /**
     * @brief Updates bounding box with a vertex position.
     */
    void UpdateBounds(const XMFLOAT3& pos)
    {
        minBounds.x = std::min(minBounds.x, pos.x);
        minBounds.y = std::min(minBounds.y, pos.y);
        minBounds.z = std::min(minBounds.z, pos.z);
        maxBounds.x = std::max(maxBounds.x, pos.x);
        maxBounds.y = std::max(maxBounds.y, pos.y);
        maxBounds.z = std::max(maxBounds.z, pos.z);
    }

    /**
     * @brief Computes the center from bounding box.
     */
    void ComputeCenter()
    {
        center.x = (minBounds.x + maxBounds.x) * 0.5f;
        center.y = (minBounds.y + maxBounds.y) * 0.5f;
        center.z = (minBounds.z + maxBounds.z) * 0.5f;
    }

    /**
     * @brief Checks if this submesh has skinning data.
     */
    bool HasSkinning() const
    {
        return !boneGroups.groupSizes.empty() && !boneGroups.skeletonBoneIndices.empty();
    }
};

/**
 * @brief Parser for FA0 geometry chunks (standard format).
 *
 * FA0 chunks contain:
 * - Submesh headers with vertex format, index/vertex counts
 * - Index buffers (multiple LOD levels)
 * - Vertex buffers with per-vertex bone indices
 * - Bone group data mapping vertex bone indices to skeleton
 */
class FA0GeometryParser
{
public:
    /**
     * @brief Decodes FVF (Flexible Vertex Format) to actual format.
     */
    static uint32_t DecodeFVF(uint32_t datFvf)
    {
        return ((datFvf & 0xff0) << 4) | ((datFvf >> 8) & 0x30) | (datFvf & 0xf);
    }

    /**
     * @brief Calculates vertex size from FVF.
     */
    static uint32_t GetVertexSize(uint32_t datFvf)
    {
        uint32_t fvf = DecodeFVF(datFvf);
        return FVF_POSITION_SIZE[fvf & 0xf] +
               FVF_NORMAL_SIZE[(fvf >> 4) & 0x7] +
               FVF_TEXCOORD_SIZE[(fvf >> 8) & 0xf] +
               FVF_TEXCOORD_SIZE[(fvf >> 12) & 0xf];
    }

    /**
     * @brief Checks if position format includes bone index.
     *
     * Position formats that include bone index after XYZ:
     * - Format 3: XYZ (12B) + bone_idx (4B) = 16B
     * - Format 5: XYZ (12B) + 2 weights (8B) + bone_idx (4B) = 24B
     * - Format 7: XYZ (12B) + bone_idx (4B) + normal (12B) = 28B
     */
    static bool PositionHasBoneIndex(uint32_t datFvf)
    {
        uint32_t fvf = DecodeFVF(datFvf);
        uint32_t posType = fvf & 0xf;
        return FVF_POSITION_SIZE[posType] > 12;
    }

    /**
     * @brief Parses a single submesh from FA0 data.
     *
     * @param data Pointer to submesh header in chunk data.
     * @param dataSize Remaining data size.
     * @param outBytesRead Receives number of bytes consumed.
     * @return Parsed submesh, or nullopt on failure.
     */
    static std::optional<ParsedSubmesh> ParseSubmesh(
        const uint8_t* data, size_t dataSize, size_t& outBytesRead)
    {
        // Submesh header: 36 bytes
        // [0]:  padding/flags
        // [4]:  idx_count_lod0
        // [8]:  idx_count_lod1
        // [12]: idx_count_lod2
        // [16]: vertex_count
        // [20]: vertex_format (FVF)
        // [24]: bone_group_count
        // [28]: total_bone_refs
        // [32]: tri_group_count

        if (dataSize < 36)
        {
            return std::nullopt;
        }

        uint32_t idxCountLod0, idxCountLod1, idxCountLod2;
        uint32_t vertexCount, vertexFormat;
        uint32_t boneGroupCount, totalBoneRefs, triGroupCount;

        std::memcpy(&idxCountLod0, &data[4], sizeof(uint32_t));
        std::memcpy(&idxCountLod1, &data[8], sizeof(uint32_t));
        std::memcpy(&idxCountLod2, &data[12], sizeof(uint32_t));
        std::memcpy(&vertexCount, &data[16], sizeof(uint32_t));
        std::memcpy(&vertexFormat, &data[20], sizeof(uint32_t));
        std::memcpy(&boneGroupCount, &data[24], sizeof(uint32_t));
        std::memcpy(&totalBoneRefs, &data[28], sizeof(uint32_t));
        std::memcpy(&triGroupCount, &data[32], sizeof(uint32_t));

        size_t offset = 36;

        // Calculate total indices
        uint32_t totalIndices = idxCountLod0;
        if (idxCountLod1 != idxCountLod0) totalIndices += idxCountLod1;
        if (idxCountLod2 != idxCountLod1) totalIndices += idxCountLod2;

        uint32_t vertexSize = GetVertexSize(vertexFormat);
        uint32_t fvf = DecodeFVF(vertexFormat);
        uint32_t posType = fvf & 0xf;
        uint32_t posSize = FVF_POSITION_SIZE[posType];
        bool hasBoneIndex = posSize > 12;

        // Validate sizes
        size_t indicesSize = totalIndices * 2;
        size_t verticesSize = vertexCount * vertexSize;
        size_t boneGroupsSize = boneGroupCount * 4;
        size_t boneRefsSize = totalBoneRefs * 4;
        size_t triGroupsSize = triGroupCount * 12;

        if (offset + indicesSize + verticesSize + boneGroupsSize + boneRefsSize + triGroupsSize > dataSize)
        {
            return std::nullopt;
        }

        ParsedSubmesh submesh;

        // Read indices (only LOD0 for now)
        submesh.indices.reserve(idxCountLod0);
        for (uint32_t i = 0; i < idxCountLod0; i++)
        {
            uint16_t idx;
            std::memcpy(&idx, &data[offset + i * 2], sizeof(uint16_t));
            submesh.indices.push_back(idx);
        }
        offset += indicesSize;

        // Read vertices
        submesh.vertices.reserve(vertexCount);
        std::vector<uint32_t> vertexBoneGroupIndices;
        vertexBoneGroupIndices.reserve(vertexCount);

        for (uint32_t i = 0; i < vertexCount; i++)
        {
            const uint8_t* vertexData = &data[offset + i * vertexSize];
            SkinnedGWVertex vertex;

            // Read position (always at offset 0)
            float x, y, z;
            std::memcpy(&x, &vertexData[0], sizeof(float));
            std::memcpy(&y, &vertexData[4], sizeof(float));
            std::memcpy(&z, &vertexData[8], sizeof(float));
            vertex.position = {x, y, -z};  // Negate Z for GW coordinate system

            submesh.UpdateBounds(vertex.position);

            // Read bone index if present (at offset 12)
            if (hasBoneIndex)
            {
                uint32_t boneGroupIdx;
                std::memcpy(&boneGroupIdx, &vertexData[12], sizeof(uint32_t));
                vertexBoneGroupIndices.push_back(boneGroupIdx);
            }
            else
            {
                vertexBoneGroupIndices.push_back(0);
            }

            // TODO: Read normals and texture coordinates based on FVF
            // For now, set defaults
            vertex.normal = {0, 1, 0};
            vertex.tex_coord0 = {0, 0};

            submesh.vertices.push_back(vertex);
        }
        offset += verticesSize;

        // Read bone groups
        submesh.boneGroups.groupSizes.reserve(boneGroupCount);
        for (uint32_t i = 0; i < boneGroupCount; i++)
        {
            uint32_t groupSize;
            std::memcpy(&groupSize, &data[offset + i * 4], sizeof(uint32_t));
            submesh.boneGroups.groupSizes.push_back(groupSize);
        }
        offset += boneGroupsSize;

        // Read skeleton bone indices
        submesh.boneGroups.skeletonBoneIndices.reserve(totalBoneRefs);
        for (uint32_t i = 0; i < totalBoneRefs; i++)
        {
            uint32_t boneId;
            std::memcpy(&boneId, &data[offset + i * 4], sizeof(uint32_t));
            submesh.boneGroups.skeletonBoneIndices.push_back(boneId);
        }
        offset += boneRefsSize;

        // Build group mapping
        submesh.boneGroups.BuildGroupMapping();

        // Map vertex bone groups to skeleton bones
        for (size_t i = 0; i < submesh.vertices.size(); i++)
        {
            uint32_t groupIdx = vertexBoneGroupIndices[i];
            uint32_t skelBone = submesh.boneGroups.MapGroupToSkeletonBone(groupIdx);
            submesh.vertices[i].SetSingleBone(skelBone);
        }

        // Skip triangle groups
        offset += triGroupsSize;

        submesh.ComputeCenter();
        outBytesRead = offset;

        return submesh;
    }
};

/**
 * @brief Parser for BB8 geometry chunks (other format).
 *
 * BB8 uses a different internal format than FA0, with inline textures
 * and a different submesh structure. See FFNA_ModelFile_Other.h for
 * the full implementation.
 */
class BB8GeometryParser
{
public:
    // BB8 header flags
    static constexpr uint32_t FLAG_BONE_GROUPS = 0x002;
    static constexpr uint32_t FLAG_BOUNDING_BOX = 0x004;
    static constexpr uint32_t FLAG_SUBMESH_DATA = 0x008;
    static constexpr uint32_t FLAG_LOD_DATA = 0x010;
    static constexpr uint32_t FLAG_VERTEX_BUFFER = 0x020;
    static constexpr uint32_t FLAG_BONE_WEIGHTS = 0x040;
    static constexpr uint32_t FLAG_MORPH_TARGETS = 0x080;
    static constexpr uint32_t FLAG_ANIMATION = 0x100;
    static constexpr uint32_t FLAG_SKELETON = 0x200;
    static constexpr uint32_t FLAG_EXTENDED_LOD = 0x400;

    /**
     * @brief BB8 header structure (48 bytes).
     */
#pragma pack(push, 1)
    struct Header
    {
        uint32_t field_0x00;
        uint32_t field_0x04;
        uint32_t classFlags;
        uint32_t signature0;
        uint32_t signature1;
        uint32_t field_0x14;
        uint8_t numBoneGroups;
        uint8_t numTextureGroups;
        uint16_t numTextures;
        uint8_t numBoneWeights;
        uint8_t numBoneIndices;
        uint16_t numMaterials;
        uint32_t numBoneWeightSets;
        uint32_t classFlagsOutput;
        float scaleX;
        float scaleY;
    };
#pragma pack(pop)

    static_assert(sizeof(Header) == 0x30, "BB8 Header must be 48 bytes!");

    /**
     * @brief Parses BB8 header to extract bone group count.
     */
    static bool ParseHeader(const uint8_t* data, size_t dataSize, Header& outHeader)
    {
        if (dataSize < sizeof(Header))
        {
            return false;
        }
        std::memcpy(&outHeader, data, sizeof(Header));
        return true;
    }
};

} // namespace GW::Parsers
