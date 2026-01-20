#pragma once

#include "AnimationClip.h"
#include "Skeleton.h"
#include "../Parsers/VLEDecoder.h"
#include "../FFNA_ModelFile_Other.h"  // For LogBB8Debug
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

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
     * Since BB9 animation data stores ABSOLUTE bind positions (not relative to parent),
     * we must compute the relative offset from parent before rotating:
     *   offset = childBasePos - parentBasePos
     *   localOffset = offset + animDelta
     *   worldPos = parentWorldPos + rotate(localOffset, parentWorldRot)
     *
     * This ensures bones stay at their correct positions in bind pose and animate properly.
     *
     * @param clip Animation clip to evaluate.
     * @param time Animation time.
     * @param outWorldPositions World positions for each bone.
     * @param outWorldRotations World rotations for each bone.
     */
    void EvaluateHierarchical(const AnimationClip& clip, float time,
                              std::vector<XMFLOAT3>& outWorldPositions,
                              std::vector<XMFLOAT4>& outWorldRotations)
    {
        size_t boneCount = clip.boneTracks.size();
        outWorldPositions.resize(boneCount);
        outWorldRotations.resize(boneCount);

        // Precompute bind pose offsets from parent (since basePositions are absolute)
        std::vector<XMFLOAT3> bindOffsets(boneCount);
        for (size_t i = 0; i < boneCount; i++)
        {
            int32_t parentIdx = (i < clip.boneParents.size()) ? clip.boneParents[i] : -1;
            if (parentIdx >= 0 && parentIdx < static_cast<int32_t>(boneCount))
            {
                const XMFLOAT3& childPos = clip.boneTracks[i].basePosition;
                const XMFLOAT3& parentPos = clip.boneTracks[parentIdx].basePosition;
                bindOffsets[i] = {
                    childPos.x - parentPos.x,
                    childPos.y - parentPos.y,
                    childPos.z - parentPos.z
                };
            }
            else
            {
                bindOffsets[i] = clip.boneTracks[i].basePosition;
            }
        }

        // Evaluate each bone
        for (size_t i = 0; i < boneCount; i++)
        {
            BoneTransform localTransform = EvaluateBoneTrack(clip.boneTracks[i], time);
            int32_t parentIdx = (i < clip.boneParents.size()) ? clip.boneParents[i] : -1;

            if (parentIdx >= 0 && parentIdx < static_cast<int32_t>(i))
            {
                // Get parent's accumulated rotation and position
                const XMFLOAT4& parentRot = outWorldRotations[parentIdx];
                const XMFLOAT3& parentPos = outWorldPositions[parentIdx];

                // Local offset = relative bind offset + animation position delta
                XMFLOAT3 localOffset = {
                    bindOffsets[i].x + localTransform.position.x,
                    bindOffsets[i].y + localTransform.position.y,
                    bindOffsets[i].z + localTransform.position.z
                };

                // Rotate local offset by parent's rotation
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
                // Root bone: world pos = bind pos + animation position delta
                const XMFLOAT3& bindPos = clip.boneTracks[i].basePosition;
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
     * Both animation and mesh use the same coordinate transform: (x, -z, y) from GW's coords.
     *
     * Skinning formula (from working Python reference):
     *   skinned_vertex = world_pos + rotate(vertex - bind_pos, world_rot)
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
        std::vector<XMFLOAT3> worldPositions;
        std::vector<XMFLOAT4> worldRotations;
        EvaluateHierarchical(clip, time, worldPositions, worldRotations);

        size_t boneCount = clip.boneTracks.size();
        outSkinningMatrices.resize(boneCount);

        for (size_t i = 0; i < boneCount; i++)
        {
            // Use custom bind position if available, otherwise fall back to animation
            const XMFLOAT3& bindPos = (i < customBindPositions.size()) ?
                customBindPositions[i] : clip.boneTracks[i].basePosition;

            const XMFLOAT3& worldPos = worldPositions[i];
            const XMFLOAT4& worldRot = worldRotations[i];

            // Skinning formula:
            // skinned = world_pos + rotate(vertex - bind_pos, world_rot)
            // Both animation and mesh now use (x, -z, y) - same coordinate space
            XMMATRIX inverseBind = XMMatrixTranslation(-bindPos.x, -bindPos.y, -bindPos.z);
            XMMATRIX boneRotation = XMMatrixRotationQuaternion(XMLoadFloat4(&worldRot));
            XMMATRIX boneTranslation = XMMatrixTranslation(worldPos.x, worldPos.y, worldPos.z);

            XMMATRIX skinning = inverseBind * boneRotation * boneTranslation;
            XMStoreFloat4x4(&outSkinningMatrices[i], skinning);
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
