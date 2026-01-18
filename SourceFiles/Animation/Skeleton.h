#pragma once

#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <unordered_map>

using namespace DirectX;

namespace GW::Animation {

/**
 * @brief Represents a single bone in a skeletal hierarchy.
 *
 * Bind pose positions are absolute world coordinates from BB9 animation data.
 * The hierarchy is reconstructed from depth values stored in the animation file.
 */
struct Bone
{
    uint32_t id = 0;                                    // Bone identifier
    XMFLOAT3 bindPosition = {0.0f, 0.0f, 0.0f};        // Absolute world position in bind pose (from BB9)
    XMFLOAT4 bindRotation = {0.0f, 0.0f, 0.0f, 1.0f}; // Identity quaternion in bind pose (w,x,y,z)
    XMFLOAT3 bindScale = {1.0f, 1.0f, 1.0f};          // Scale in bind pose
    uint8_t hierarchyDepth = 0;                        // Depth in bone hierarchy (used to reconstruct parent-child)
    int32_t parentIndex = -1;                          // Index of parent bone (-1 for root bones)
    XMFLOAT4X4 inverseBindMatrix;                      // Inverse of bind pose matrix for skinning

    Bone()
    {
        XMStoreFloat4x4(&inverseBindMatrix, XMMatrixIdentity());
    }

    /**
     * @brief Computes the local bind pose matrix for this bone.
     * @return Transformation matrix from local to parent space in bind pose.
     */
    XMMATRIX GetBindPoseMatrix() const
    {
        XMMATRIX translation = XMMatrixTranslation(bindPosition.x, bindPosition.y, bindPosition.z);
        XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&bindRotation));
        XMMATRIX scale = XMMatrixScaling(bindScale.x, bindScale.y, bindScale.z);
        return scale * rotation * translation;
    }
};

/**
 * @brief Complete skeleton structure containing all bones and hierarchy information.
 *
 * Hierarchy is built from depth values using the following rules:
 * 1. Depth increased from previous bone → previous bone is parent (child relationship)
 * 2. Depth decreased or stayed same → look up bone at depth-1 (sibling relationship)
 * 3. Multiple bones at same depth are siblings (share same parent)
 */
struct Skeleton
{
    std::vector<Bone> bones;
    std::vector<int32_t> boneParents;  // Redundant with Bone::parentIndex but useful for quick lookup

    // Maps bone ID to bone index for fast lookup
    std::unordered_map<uint32_t, size_t> boneIdToIndex;

    /**
     * @brief Reconstructs the bone hierarchy from depth values.
     *
     * This algorithm is ported from animated_viewer.py and matches
     * how Guild Wars stores bone hierarchies:
     * - Depth values indicate nesting level in the tree
     * - Parent relationships are inferred from depth changes
     */
    void ComputeHierarchyFromDepths()
    {
        boneParents.resize(bones.size(), -1);
        std::unordered_map<uint8_t, int32_t> depthToBone;
        int prevDepth = -1;

        for (size_t i = 0; i < bones.size(); i++)
        {
            if (i == 0)
            {
                // Root bone has no parent
                boneParents[i] = -1;
                bones[i].parentIndex = -1;
            }
            else if (bones[i].hierarchyDepth > prevDepth)
            {
                // Depth increased - previous bone is parent
                boneParents[i] = static_cast<int32_t>(i - 1);
                bones[i].parentIndex = static_cast<int32_t>(i - 1);
            }
            else
            {
                // Depth stayed same or decreased - look up sibling's parent
                auto it = depthToBone.find(bones[i].hierarchyDepth - 1);
                int32_t parent = (it != depthToBone.end()) ? it->second : -1;
                boneParents[i] = parent;
                bones[i].parentIndex = parent;
            }

            depthToBone[bones[i].hierarchyDepth] = static_cast<int32_t>(i);
            prevDepth = bones[i].hierarchyDepth;
        }
    }

    /**
     * @brief Computes inverse bind matrices for all bones.
     *
     * The inverse bind matrix transforms a vertex from model space to bone space.
     * This is used in linear blend skinning to transform vertices relative to
     * their bind pose position.
     */
    void ComputeInverseBindMatrices()
    {
        // First compute world-space bind pose matrices by traversing hierarchy
        std::vector<XMMATRIX> worldBindMatrices(bones.size());

        for (size_t i = 0; i < bones.size(); i++)
        {
            XMMATRIX localMatrix = bones[i].GetBindPoseMatrix();

            if (bones[i].parentIndex >= 0 && bones[i].parentIndex < static_cast<int32_t>(i))
            {
                // Multiply by parent's world matrix
                worldBindMatrices[i] = localMatrix * worldBindMatrices[bones[i].parentIndex];
            }
            else
            {
                // Root bone - local matrix is world matrix
                worldBindMatrices[i] = localMatrix;
            }

            // Compute and store inverse
            XMVECTOR det;
            XMMATRIX inverseMatrix = XMMatrixInverse(&det, worldBindMatrices[i]);
            XMStoreFloat4x4(&bones[i].inverseBindMatrix, inverseMatrix);
        }
    }

    /**
     * @brief Builds the bone ID to index lookup map.
     */
    void BuildBoneIdMap()
    {
        boneIdToIndex.clear();
        for (size_t i = 0; i < bones.size(); i++)
        {
            boneIdToIndex[bones[i].id] = i;
        }
    }

    /**
     * @brief Gets bone index by bone ID.
     * @param boneId The bone ID to look up.
     * @return Bone index, or -1 if not found.
     */
    int32_t GetBoneIndex(uint32_t boneId) const
    {
        auto it = boneIdToIndex.find(boneId);
        if (it != boneIdToIndex.end())
        {
            return static_cast<int32_t>(it->second);
        }
        return -1;
    }

    /**
     * @brief Gets number of bones in skeleton.
     */
    size_t GetBoneCount() const { return bones.size(); }

    /**
     * @brief Checks if skeleton is valid (has at least one bone).
     */
    bool IsValid() const { return !bones.empty(); }

    /**
     * @brief Computes the bind pose offset from parent for a bone.
     *
     * Since base_position is absolute in bind pose, the offset is:
     * offset = child.base_position - parent.base_position
     *
     * @param boneIndex Index of the bone.
     * @return Offset from parent bone in bind pose.
     */
    XMFLOAT3 GetBindPoseOffsetFromParent(size_t boneIndex) const
    {
        if (boneIndex >= bones.size())
        {
            return {0.0f, 0.0f, 0.0f};
        }

        const Bone& bone = bones[boneIndex];
        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int32_t>(bones.size()))
        {
            const Bone& parent = bones[bone.parentIndex];
            return {
                bone.bindPosition.x - parent.bindPosition.x,
                bone.bindPosition.y - parent.bindPosition.y,
                bone.bindPosition.z - parent.bindPosition.z
            };
        }

        // Root bone - offset is its position
        return bone.bindPosition;
    }
};

} // namespace GW::Animation
