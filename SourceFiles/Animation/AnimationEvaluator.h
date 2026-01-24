#pragma once

#include "AnimationClip.h"
#include "Skeleton.h"
#include "../Parsers/VLEDecoder.h"
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <algorithm>

using namespace DirectX;

namespace GW::Animation {

/**
 * @brief Evaluates animation clips to produce bone transforms at a given time.
 *
 * Handles:
 * - Binary search for keyframe lookup (O(log n))
 * - Linear interpolation for position and scale
 * - Spherical linear interpolation (SLERP) for quaternion rotation
 * - Hierarchical bone transform propagation
 */
class AnimationEvaluator
{
public:
    /**
     * @brief Result of animation evaluation for a single bone.
     */
    struct BoneTransform
    {
        XMFLOAT3 position = {0.0f, 0.0f, 0.0f};
        XMFLOAT4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};  // Identity quaternion (XMFLOAT4: x,y,z,w)
        XMFLOAT3 scale = {1.0f, 1.0f, 1.0f};

        /**
         * @brief Converts to a 4x4 transformation matrix (SRT order).
         */
        XMMATRIX ToMatrix() const
        {
            XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
            XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
            XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);
            return S * R * T;
        }
    };

    /**
     * @brief Evaluates all bones at a given animation time.
     *
     * @param clip Animation clip to evaluate.
     * @param time Animation time in game units.
     * @param outBoneMatrices Output array of bone matrices (world space).
     */
    void Evaluate(const AnimationClip& clip, float time, std::vector<XMFLOAT4X4>& outBoneMatrices)
    {
        size_t boneCount = clip.boneTracks.size();
        outBoneMatrices.resize(boneCount);

        // First, evaluate local transforms for all bones
        std::vector<BoneTransform> localTransforms(boneCount);
        for (size_t i = 0; i < boneCount; i++)
        {
            localTransforms[i] = EvaluateBoneTrack(clip.boneTracks[i], time);
        }

        // Then compute world transforms using hierarchy
        std::vector<XMMATRIX> worldMatrices(boneCount);
        for (size_t i = 0; i < boneCount; i++)
        {
            XMMATRIX localMatrix = localTransforms[i].ToMatrix();

            int32_t parentIdx = (i < clip.boneParents.size()) ? clip.boneParents[i] : -1;
            if (parentIdx >= 0 && parentIdx < static_cast<int32_t>(i))
            {
                worldMatrices[i] = localMatrix * worldMatrices[parentIdx];
            }
            else
            {
                worldMatrices[i] = localMatrix;
            }

            XMStoreFloat4x4(&outBoneMatrices[i], worldMatrices[i]);
        }
    }

    /**
     * @brief Evaluates animation and produces skinning matrices.
     *
     * The skinning matrix = WorldMatrix * InverseBindMatrix
     * This transforms vertices from bind pose to animated pose.
     *
     * @param clip Animation clip to evaluate.
     * @param skeleton Skeleton with inverse bind matrices.
     * @param time Animation time.
     * @param outSkinningMatrices Output array of skinning matrices.
     */
    void EvaluateForSkinning(const AnimationClip& clip, const Skeleton& skeleton,
                             float time, std::vector<XMFLOAT4X4>& outSkinningMatrices)
    {
        // Get world-space bone matrices
        std::vector<XMFLOAT4X4> worldMatrices;
        Evaluate(clip, time, worldMatrices);

        // Multiply by inverse bind matrices
        size_t boneCount = std::min(worldMatrices.size(), skeleton.bones.size());
        outSkinningMatrices.resize(boneCount);

        for (size_t i = 0; i < boneCount; i++)
        {
            XMMATRIX world = XMLoadFloat4x4(&worldMatrices[i]);
            XMMATRIX invBind = XMLoadFloat4x4(&skeleton.bones[i].inverseBindMatrix);
            XMMATRIX skinning = invBind * world;
            XMStoreFloat4x4(&outSkinningMatrices[i], skinning);
        }
    }

    /**
     * @brief Evaluates bone hierarchy to compute world-space transforms.
     *
     * Based on Ghidra RE of Model_UpdateSkeletonTransforms @ 0x00754720:
     *
     * GW's algorithm for each bone:
     * 1. Pop stack to parent level (via GrTrans_PushPopMatrix with depth byte)
     * 2. Model_ApplyBoneTransform: Push matrix with (basePos + animDelta) and rotation
     * 3. GrTrans_Translate(-basePos): Subtract base position from matrix
     *
     * Net effect: Each bone's LOCAL offset from parent is (animDelta) only.
     * The bind offset (childBasePos - parentBasePos) is implicit in the vertex data.
     *
     * For skinning, we compute world transforms as:
     *   worldPos = parentWorldPos + rotate(bindOffset + animDelta, parentWorldRot)
     *   worldRot = parentWorldRot * localRot
     *
     * IMPORTANT: For POP_COUNT mode models, the animation's basePosition values may not
     * match the mesh's actual bind positions. In such cases, pass customBindPositions
     * (derived from mesh vertex centroids) to get correct world transforms.
     *
     * @param clip Animation clip to evaluate.
     * @param time Animation time.
     * @param outWorldPositions World positions for each bone.
     * @param outWorldRotations World rotations for each bone.
     * @param customBindPositions Optional custom bind positions for computing bind offsets.
     *                            If provided, these are used instead of animation basePosition.
     *                            This is essential for POP_COUNT mode models where animation
     *                            basePosition doesn't match mesh bind positions.
     */
    void EvaluateHierarchical(const AnimationClip& clip, float time,
                              std::vector<XMFLOAT3>& outWorldPositions,
                              std::vector<XMFLOAT4>& outWorldRotations,
                              const std::vector<XMFLOAT3>* customBindPositions = nullptr)
    {
        size_t boneCount = clip.boneTracks.size();
        outWorldPositions.resize(boneCount);
        outWorldRotations.resize(boneCount);

        // Helper to get bind position - uses custom if available, otherwise animation
        auto getBindPosition = [&](size_t idx) -> XMFLOAT3 {
            if (customBindPositions && idx < customBindPositions->size())
            {
                return (*customBindPositions)[idx];
            }
            return clip.boneTracks[idx].basePosition;
        };

        // Precompute bind pose offsets from parent
        // Use custom bind positions if provided (essential for POP_COUNT mode)
        std::vector<XMFLOAT3> bindOffsets(boneCount);
        for (size_t i = 0; i < boneCount; i++)
        {
            int32_t parentIdx = (i < clip.boneParents.size()) ? clip.boneParents[i] : -1;
            if (parentIdx >= 0 && parentIdx < static_cast<int32_t>(boneCount))
            {
                const XMFLOAT3 childPos = getBindPosition(i);
                const XMFLOAT3 parentPos = getBindPosition(static_cast<size_t>(parentIdx));
                bindOffsets[i] = {
                    childPos.x - parentPos.x,
                    childPos.y - parentPos.y,
                    childPos.z - parentPos.z
                };
            }
            else
            {
                // Root bone or no parent - use absolute position
                bindOffsets[i] = getBindPosition(i);
            }
        }

        // Evaluate each bone
        for (size_t i = 0; i < boneCount; i++)
        {
            BoneTransform localTransform = EvaluateBoneTrack(clip.boneTracks[i], time);
            int32_t parentIdx = (i < clip.boneParents.size()) ? clip.boneParents[i] : -1;

            if (parentIdx < 0)
            {
                // ROOT BONE: Absolute position and rotation
                const XMFLOAT3 bindPos = getBindPosition(i);
                outWorldPositions[i] = {
                    bindPos.x + localTransform.position.x,
                    bindPos.y + localTransform.position.y,
                    bindPos.z + localTransform.position.z
                };
                outWorldRotations[i] = localTransform.rotation;
            }
            else if (parentIdx < static_cast<int32_t>(i))
            {
                // CHILD BONE: Position is always hierarchical
                const XMFLOAT3& parentPos = outWorldPositions[parentIdx];

                // Local offset = bind offset (relative to parent) + animation delta
                XMFLOAT3 localOffset = {
                    bindOffsets[i].x + localTransform.position.x,
                    bindOffsets[i].y + localTransform.position.y,
                    bindOffsets[i].z + localTransform.position.z
                };

                // ALWAYS hierarchical: rotate offset by parent rotation, accumulate rotations
                // Based on Ghidra RE: GW ALWAYS uses matrix stack multiplication
                // The pop count controls HOW MANY levels to pop, NOT whether rotations accumulate
                const XMFLOAT4& parentRot = outWorldRotations[parentIdx];

                // Rotate local offset by parent's world rotation
                XMFLOAT3 rotatedOffset = Parsers::VLEDecoder::QuaternionRotatePoint(parentRot, localOffset);

                // World position = parent position + rotated offset
                outWorldPositions[i] = {
                    parentPos.x + rotatedOffset.x,
                    parentPos.y + rotatedOffset.y,
                    parentPos.z + rotatedOffset.z
                };

                // World rotation = parent rotation * local rotation
                outWorldRotations[i] = Parsers::VLEDecoder::QuaternionMultiply(parentRot, localTransform.rotation);
            }
            else
            {
                // Forward reference (parent not yet processed) - treat as root
                const XMFLOAT3 bindPos = getBindPosition(i);
                outWorldPositions[i] = {
                    bindPos.x + localTransform.position.x,
                    bindPos.y + localTransform.position.y,
                    bindPos.z + localTransform.position.z
                };
                outWorldRotations[i] = localTransform.rotation;
            }
        }
    }

    /**
     * @brief Computes skinning matrices using hierarchical evaluation.
     *
     * For each vertex:
     * 1. Compute vertex offset from bone's bind position
     * 2. Rotate offset by bone's world rotation
     * 3. Add bone's world position
     *
     * Both animation and mesh use the same coordinate transform: (x, -z, y) from GW's coords.
     * GW uses (left/right, front/back, down/up), GWMB uses (left/right, up/down, front/back).
     */
    void ComputeSkinningFromHierarchy(const AnimationClip& clip, float time,
                                      std::vector<XMFLOAT4X4>& outSkinningMatrices)
    {
        // Use animation bind positions directly
        std::vector<XMFLOAT3> bindPositions;
        bindPositions.reserve(clip.boneTracks.size());
        for (const auto& track : clip.boneTracks)
        {
            bindPositions.push_back(track.basePosition);
        }
        ComputeSkinningWithCustomBindPositions(clip, time, bindPositions, outSkinningMatrices);
    }

    /**
     * @brief Computes skinning matrices using custom bind positions (e.g., mesh-derived).
     *
     * Use this method when the animation bind positions don't match the mesh vertices.
     * Pass mesh-derived bind positions (centroid of vertices per bone) for correct skinning.
     *
     * This implements GW's exact algorithm from Ghidra RE (Model_UpdateSkeletonTransforms @ 0x00754720):
     *
     * For each bone:
     * 1. Build local matrix: M_local = R_local * T(bindPos + animDelta)
     * 2. Multiply with parent: M_accumulated = M_local * M_parent
     * 3. Apply bind offset: M_bone = T(-bindPos) * M_accumulated
     *
     * Skinning: V' = M_bone * V
     *
     * @param clip Animation clip to evaluate.
     * @param time Animation time.
     * @param customBindPositions Custom bind positions (typically derived from mesh vertex centroids).
     * @param outSkinningMatrices Output array of skinning matrices.
     */
    void ComputeSkinningWithCustomBindPositions(const AnimationClip& clip, float time,
                                                 const std::vector<XMFLOAT3>& customBindPositions,
                                                 std::vector<XMFLOAT4X4>& outSkinningMatrices)
    {
        // Evaluate hierarchical transforms
        std::vector<XMFLOAT3> worldPositions;
        std::vector<XMFLOAT4> worldRotations;
        EvaluateHierarchical(clip, time, worldPositions, worldRotations, nullptr);

        size_t boneCount = clip.boneTracks.size();

        // Ghidra RE (Model_UpdateSkeletonTransforms @ 0x00754720):
        // Bones with flag 0x10000000 are "intermediate" - they participate in hierarchy
        // but DON'T produce output skinning matrices. Mesh vertices reference OUTPUT
        // indices which skip intermediate bones.
        size_t outputBoneCount = clip.GetOutputBoneCount();
        bool hasIntermediateBones = (outputBoneCount > 0 && outputBoneCount < boneCount);

        outSkinningMatrices.resize(hasIntermediateBones ? outputBoneCount : boneCount);

        for (size_t i = 0; i < boneCount; i++)
        {
            // Skip intermediate bones - they don't produce output matrices
            if (hasIntermediateBones)
            {
                int32_t outputIdx = clip.GetOutputFromAnimBone(static_cast<uint32_t>(i));
                if (outputIdx < 0)
                {
                    continue;
                }
            }

            // Mesh bind position - use output index for lookup when intermediate bones exist
            size_t meshBindIdx = i;
            if (hasIntermediateBones)
            {
                int32_t outputIdx = clip.GetOutputFromAnimBone(static_cast<uint32_t>(i));
                if (outputIdx >= 0)
                {
                    meshBindIdx = static_cast<size_t>(outputIdx);
                }
            }

            const XMFLOAT3& meshBindPos = (meshBindIdx < customBindPositions.size()) ?
                customBindPositions[meshBindIdx] : clip.boneTracks[i].basePosition;
            const XMFLOAT3& animBindPos = clip.boneTracks[i].basePosition;
            const XMFLOAT3& worldPos = worldPositions[i];
            const XMFLOAT4& worldRot = worldRotations[i];

            // GW skinning: M = T(-meshBindPos) * R(worldRot) * T(finalBonePos)
            // where finalBonePos = worldPos + R(worldRot) * (meshBindPos - animBindPos)
            XMFLOAT3 boneOffset = {
                meshBindPos.x - animBindPos.x,
                meshBindPos.y - animBindPos.y,
                meshBindPos.z - animBindPos.z
            };

            XMFLOAT3 rotatedOffset = Parsers::VLEDecoder::QuaternionRotatePoint(worldRot, boneOffset);

            XMFLOAT3 finalBonePos = {
                worldPos.x + rotatedOffset.x,
                worldPos.y + rotatedOffset.y,
                worldPos.z + rotatedOffset.z
            };

            XMMATRIX inverseBind = XMMatrixTranslation(-meshBindPos.x, -meshBindPos.y, -meshBindPos.z);
            XMMATRIX boneRotation = XMMatrixRotationQuaternion(XMLoadFloat4(&worldRot));
            XMMATRIX boneTranslation = XMMatrixTranslation(finalBonePos.x, finalBonePos.y, finalBonePos.z);

            XMMATRIX skinning = inverseBind * boneRotation * boneTranslation;

            // Store at OUTPUT index, not animation bone index
            size_t storeIdx = i;
            if (hasIntermediateBones)
            {
                int32_t outputIdx = clip.GetOutputFromAnimBone(static_cast<uint32_t>(i));
                if (outputIdx >= 0)
                {
                    storeIdx = static_cast<size_t>(outputIdx);
                }
            }
            XMStoreFloat4x4(&outSkinningMatrices[storeIdx], skinning);
        }
    }

private:
    /**
     * @brief Evaluates a single bone track at a given time.
     */
    BoneTransform EvaluateBoneTrack(const BoneTrack& track, float time)
    {
        BoneTransform result;

        // Interpolate position
        if (!track.positionKeys.empty())
        {
            result.position = InterpolateVec3(track.positionKeys, time);
        }

        // Interpolate rotation
        if (!track.rotationKeys.empty())
        {
            result.rotation = InterpolateQuat(track.rotationKeys, time);
        }

        // Interpolate scale
        if (!track.scaleKeys.empty())
        {
            result.scale = InterpolateVec3(track.scaleKeys, time);
        }

        return result;
    }

    /**
     * @brief Binary search to find keyframe index and interpolation factor.
     *
     * @tparam T Keyframe value type.
     * @param keys Sorted keyframe array.
     * @param time Target time.
     * @return Pair of (index, interpolation factor 0-1).
     */
    template<typename T>
    std::pair<size_t, float> FindKeyframe(const std::vector<Keyframe<T>>& keys, float time)
    {
        if (keys.empty())
        {
            return {0, 0.0f};
        }

        if (keys.size() == 1 || time <= keys.front().time)
        {
            return {0, 0.0f};
        }

        if (time >= keys.back().time)
        {
            return {keys.size() - 2, 1.0f};
        }

        // Binary search
        size_t lo = 0, hi = keys.size() - 1;
        while (hi - lo > 1)
        {
            size_t mid = (lo + hi) / 2;
            if (keys[mid].time <= time)
            {
                lo = mid;
            }
            else
            {
                hi = mid;
            }
        }

        // Calculate interpolation factor
        float t1 = keys[lo].time;
        float t2 = keys[lo + 1].time;
        float t = (t2 > t1) ? (time - t1) / (t2 - t1) : 0.0f;

        return {lo, t};
    }

    /**
     * @brief Linear interpolation for vec3 values.
     */
    XMFLOAT3 InterpolateVec3(const std::vector<Keyframe<XMFLOAT3>>& keys, float time)
    {
        if (keys.empty())
        {
            return {0.0f, 0.0f, 0.0f};
        }

        auto [idx, t] = FindKeyframe(keys, time);

        if (idx >= keys.size() - 1)
        {
            return keys.back().value;
        }

        const XMFLOAT3& v1 = keys[idx].value;
        const XMFLOAT3& v2 = keys[idx + 1].value;

        return {
            v1.x + t * (v2.x - v1.x),
            v1.y + t * (v2.y - v1.y),
            v1.z + t * (v2.z - v1.z)
        };
    }

    /**
     * @brief Spherical linear interpolation for quaternions.
     */
    XMFLOAT4 InterpolateQuat(const std::vector<Keyframe<XMFLOAT4>>& keys, float time)
    {
        if (keys.empty())
        {
            return {0.0f, 0.0f, 0.0f, 1.0f};
        }

        auto [idx, t] = FindKeyframe(keys, time);

        if (idx >= keys.size() - 1)
        {
            return keys.back().value;
        }

        return Parsers::VLEDecoder::QuaternionSlerp(keys[idx].value, keys[idx + 1].value, t);
    }
};

} // namespace GW::Animation
