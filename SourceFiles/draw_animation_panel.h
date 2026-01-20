#pragma once

#include "Animation/AnimationController.h"
#include "AnimatedMeshInstance.h"
#include "Vertex.h"
#include "Mesh.h"
#include "FFNA_ModelFile_Other.h"  // For LogBB8Debug
#include <DirectXMath.h>
#include <memory>
#include <map>
#include <vector>
#include <atomic>
#include <string>
#include <cstring>

// Forward declarations
class DATManager;

/**
 * @brief Result of an animation search - a file that contains matching animation data.
 */
struct AnimationSearchResult
{
    uint32_t fileId = 0;        // File ID (hash)
    int mftIndex = -1;          // Index in MFT for loading
    int datAlias = 0;           // Which DAT file it's from
    uint32_t sequenceCount = 0; // Number of animation sequences
    uint32_t boneCount = 0;     // Number of bones
    std::string chunkType;      // "BB9" or "FA1"
};

/**
 * @brief Visualization options for animated model rendering.
 */
struct AnimationVisualizationOptions
{
    // Mesh rendering options
    bool showMesh = true;
    bool wireframeMode = false;
    float meshAlpha = 1.0f;  // 0.0 to 1.0

    // Bone visualization
    bool showBones = false;
    float boneLineWidth = 2.0f;
    float jointRadius = 50.0f;  // Radius of joint spheres (GW uses large coordinate scales)
    DirectX::XMFLOAT4 boneColor = {1.0f, 1.0f, 0.0f, 1.0f};  // Yellow
    DirectX::XMFLOAT4 jointColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red

    // Debug options
    bool disableSkinning = false;  // If true, render mesh without animation skinning (bind pose)

    // Submesh visibility (indexed by submesh ID)
    std::vector<bool> submeshVisibility;

    void ResetSubmeshVisibility(size_t count)
    {
        submeshVisibility.clear();
        submeshVisibility.resize(count, true);
    }

    bool IsSubmeshVisible(size_t idx) const
    {
        return idx < submeshVisibility.size() ? submeshVisibility[idx] : true;
    }
};

/**
 * @brief Global animation state for the UI to control.
 *
 * This structure holds the animation controller and related state
 * that the animation panel UI can interact with.
 */
struct AnimationPanelState
{
    std::shared_ptr<GW::Animation::AnimationController> controller;
    std::shared_ptr<GW::Animation::AnimationClip> clip;
    std::shared_ptr<GW::Animation::Skeleton> skeleton;

    uint32_t currentFileId = 0;  // File ID of the currently loaded animation/model
    bool hasAnimation = false;   // Whether animation data is available
    bool hasModel = false;       // Whether a model is loaded (for hash display)

    // Model hashes for finding matching animations (from BB8/FA0 geometry chunk)
    uint32_t modelHash0 = 0;
    uint32_t modelHash1 = 0;

    // Animation search state
    std::vector<AnimationSearchResult> searchResults;
    std::atomic<bool> searchInProgress{false};
    std::atomic<int> filesProcessed{0};
    std::atomic<int> totalFiles{0};
    int selectedResultIndex = -1;

    // Visualization options
    AnimationVisualizationOptions visualization;

    // Persistent playback settings (survive Reset() and animation switches)
    struct PlaybackSettings
    {
        float playbackSpeed = 1.0f;
        bool looping = true;
        bool autoCycle = true;  // Default to enabled
        bool hasBeenSet = false;  // True once user has changed any setting
    } playbackSettings;

    // Submesh information (populated when model is loaded)
    std::vector<std::string> submeshNames;
    size_t submeshCount = 0;

    // Animated mesh instances (created when animation is loaded with bone data)
    std::vector<std::shared_ptr<AnimatedMeshInstance>> animatedMeshes;
    bool hasSkinnedMeshes = false;

    // Bone group mapping per submesh (for mapping vertex bone groups to skeleton bones)
    struct SubmeshBoneData
    {
        std::vector<uint32_t> groupSizes;           // Size of each bone group
        std::vector<uint32_t> skeletonBoneIndices;  // Flat array of skeleton bone IDs
        std::vector<uint32_t> groupToSkeletonBone;  // Mapping: group index -> first skeleton bone

        void BuildGroupMapping()
        {
            groupToSkeletonBone.clear();
            size_t skelIdx = 0;
            for (size_t i = 0; i < groupSizes.size(); i++)
            {
                if (skelIdx < skeletonBoneIndices.size())
                {
                    groupToSkeletonBone.push_back(skeletonBoneIndices[skelIdx]);
                }
                else
                {
                    groupToSkeletonBone.push_back(0);
                }
                skelIdx += groupSizes[i];
            }
        }

        uint32_t MapGroupToSkeletonBone(uint32_t groupIdx) const
        {
            return groupIdx < groupToSkeletonBone.size() ? groupToSkeletonBone[groupIdx] : 0;
        }
    };
    std::vector<SubmeshBoneData> submeshBoneData;

    // Per-vertex bone group indices for each submesh (needed for skinning)
    std::vector<std::vector<uint32_t>> perVertexBoneGroups;

    // Store original meshes for skinned rendering
    std::vector<Mesh> originalMeshes;

    // Store mesh IDs for submesh visibility control (assigned by MapRenderer)
    std::vector<int> meshIds;

    // Store per-object constant buffer data for each submesh (needed for rendering)
    std::vector<PerObjectCB> perMeshPerObjectCB;

    // Store texture IDs for each submesh (indices into texture manager)
    std::vector<std::vector<int>> perMeshTextureIds;

    void Reset()
    {
        // Save playback settings before reset (they persist across model/animation changes)
        PlaybackSettings savedSettings = playbackSettings;

        controller.reset();
        clip.reset();
        skeleton.reset();
        currentFileId = 0;
        hasAnimation = false;
        hasModel = false;
        modelHash0 = 0;
        modelHash1 = 0;
        searchResults.clear();
        selectedResultIndex = -1;
        visualization = AnimationVisualizationOptions();
        submeshNames.clear();
        submeshCount = 0;
        animatedMeshes.clear();
        hasSkinnedMeshes = false;
        submeshBoneData.clear();
        perVertexBoneGroups.clear();
        originalMeshes.clear();
        meshIds.clear();
        perMeshPerObjectCB.clear();
        perMeshTextureIds.clear();

        // Restore playback settings
        playbackSettings = savedSettings;
    }

    void SetModelHashes(uint32_t hash0, uint32_t hash1, uint32_t fileId)
    {
        modelHash0 = hash0;
        modelHash1 = hash1;
        currentFileId = fileId;
        hasModel = true;
        // Clear previous search results when model changes
        searchResults.clear();
        selectedResultIndex = -1;
    }

    void SetSubmeshInfo(size_t count, const std::vector<std::string>& names = {})
    {
        submeshCount = count;
        submeshNames = names;
        if (submeshNames.size() < count)
        {
            submeshNames.resize(count);
        }
        visualization.ResetSubmeshVisibility(count);
    }

    /**
     * @brief Extracts bone group data from a model's extra_data.
     *
     * The extra_data layout in FA0 format is:
     * - [0, u0*4): bone_group_sizes (u0 uint32_t values)
     * - [u0*4, (u0+u1)*4): skeleton_bone_indices (u1 uint32_t values)
     * - [(u0+u1)*4, end): triangle groups (u2 * 12 bytes)
     *
     * @param extra_data The raw extra data from GeometryModel
     * @param boneGroupCount Number of bone groups (u0)
     * @param totalBoneRefs Total skeleton bone refs (u1)
     * @return SubmeshBoneData with mapping built
     */
    static SubmeshBoneData ExtractBoneData(const std::vector<uint8_t>& extra_data,
                                           uint32_t boneGroupCount, uint32_t totalBoneRefs)
    {
        SubmeshBoneData boneData;

        if (extra_data.empty() || boneGroupCount == 0)
            return boneData;

        // Read bone group sizes
        size_t offset = 0;
        for (uint32_t i = 0; i < boneGroupCount && offset + 4 <= extra_data.size(); i++)
        {
            uint32_t groupSize;
            std::memcpy(&groupSize, &extra_data[offset], sizeof(uint32_t));
            boneData.groupSizes.push_back(groupSize);
            offset += 4;
        }

        // Read skeleton bone indices
        for (uint32_t i = 0; i < totalBoneRefs && offset + 4 <= extra_data.size(); i++)
        {
            uint32_t boneId;
            std::memcpy(&boneId, &extra_data[offset], sizeof(uint32_t));
            boneData.skeletonBoneIndices.push_back(boneId);
            offset += 4;
        }

        // Build the group to skeleton bone mapping
        boneData.BuildGroupMapping();

        return boneData;
    }

    /**
     * @brief Creates skinned vertices from a mesh and bone data.
     *
     * @param mesh The original mesh with GWVertex
     * @param boneData Bone group mapping for this submesh
     * @param vertexBoneGroups Per-vertex bone group indices from ModelVertex.group
     * @param boneCount Total number of bones in the skeleton (for validation)
     * @return Vector of SkinnedGWVertex with bone weights set
     */
    static std::vector<SkinnedGWVertex> CreateSkinnedVertices(
        const Mesh& mesh,
        const SubmeshBoneData& boneData,
        const std::vector<uint32_t>& vertexBoneGroups,
        size_t boneCount = 256)
    {
        std::vector<SkinnedGWVertex> skinnedVertices;
        skinnedVertices.reserve(mesh.vertices.size());

        // Determine if we should use direct bone indices or group mapping
        // If vertex bone indices exceed group count but are within bone count,
        // they might be direct skeleton bone indices
        bool useDirectIndices = false;
        uint32_t maxVertexBoneIdx = 0;
        for (const auto& idx : vertexBoneGroups)
        {
            if (idx > maxVertexBoneIdx) maxVertexBoneIdx = idx;
        }

        // If no mapping available or max index exceeds group count but is within skeleton,
        // use direct indexing
        if (boneData.groupToSkeletonBone.empty())
        {
            useDirectIndices = true;
            LogBB8Debug("  CreateSkinnedVertices: No group mapping, using direct indices\n");
        }
        else if (maxVertexBoneIdx >= boneData.groupToSkeletonBone.size() && maxVertexBoneIdx < boneCount)
        {
            useDirectIndices = true;
            char msg[256];
            sprintf_s(msg, "  CreateSkinnedVertices: maxVertexBoneIdx(%u) >= groupCount(%zu), using direct indices\n",
                maxVertexBoneIdx, boneData.groupToSkeletonBone.size());
            LogBB8Debug(msg);
        }

        for (size_t i = 0; i < mesh.vertices.size(); i++)
        {
            SkinnedGWVertex sv(mesh.vertices[i]);

            uint32_t groupIdx = i < vertexBoneGroups.size() ? vertexBoneGroups[i] : 0;
            uint32_t skelBone;

            if (useDirectIndices)
            {
                // Use vertex bone index directly as skeleton bone index
                skelBone = groupIdx < boneCount ? groupIdx : 0;
            }
            else
            {
                // Map bone group index to skeleton bone via palette
                skelBone = boneData.MapGroupToSkeletonBone(groupIdx);
            }

            sv.SetSingleBone(skelBone);
            skinnedVertices.push_back(sv);
        }

        return skinnedVertices;
    }

    void Initialize(std::shared_ptr<GW::Animation::AnimationClip> animClip,
                    std::shared_ptr<GW::Animation::Skeleton> skel,
                    uint32_t fileId)
    {
        clip = animClip;
        skeleton = skel;
        currentFileId = fileId;

        // Clear old skinned meshes so they get recreated with the new animation
        animatedMeshes.clear();
        hasSkinnedMeshes = false;

        if (clip && clip->IsValid())
        {
            controller = std::make_shared<GW::Animation::AnimationController>();
            controller->Initialize(clip, skeleton);
            hasAnimation = true;

            // Apply persistent playback settings to the new controller
            controller->SetPlaybackSpeed(playbackSettings.playbackSpeed * 100000.0f);
            controller->SetLooping(playbackSettings.looping);
            controller->SetAutoCycleSequences(playbackSettings.autoCycle);
        }
        else
        {
            hasAnimation = false;
        }
    }

    /**
     * @brief Computes mesh-derived bind positions from vertex centroids.
     *
     * Animation bind positions often don't match where mesh vertices actually are.
     * This function computes the centroid of all vertices assigned to each skeleton bone,
     * which gives accurate bind positions for skinning.
     *
     * Call this after both model meshes and animation are loaded.
     *
     * @return Vector of bind positions indexed by skeleton bone index.
     */
    std::vector<DirectX::XMFLOAT3> ComputeMeshBindPositions()
    {
        if (!hasAnimation || !clip || originalMeshes.empty())
        {
            return {};
        }

        size_t boneCount = clip->boneTracks.size();

        // Accumulate positions and counts per skeleton bone
        std::vector<DirectX::XMFLOAT3> positionSums(boneCount, {0.0f, 0.0f, 0.0f});
        std::vector<uint32_t> vertexCounts(boneCount, 0);

        // Iterate through all meshes and vertices
        for (size_t meshIdx = 0; meshIdx < originalMeshes.size(); meshIdx++)
        {
            const auto& mesh = originalMeshes[meshIdx];
            const auto& boneData = (meshIdx < submeshBoneData.size()) ?
                submeshBoneData[meshIdx] : SubmeshBoneData();
            const auto& vertexBoneGroups = (meshIdx < perVertexBoneGroups.size()) ?
                perVertexBoneGroups[meshIdx] : std::vector<uint32_t>();

            // Determine if we should use direct bone indices or group mapping
            bool useDirectIndices = false;
            uint32_t maxVertexBoneIdx = 0;
            for (const auto& idx : vertexBoneGroups)
            {
                if (idx > maxVertexBoneIdx) maxVertexBoneIdx = idx;
            }
            if (boneData.groupToSkeletonBone.empty() ||
                (maxVertexBoneIdx >= boneData.groupToSkeletonBone.size() && maxVertexBoneIdx < boneCount))
            {
                useDirectIndices = true;
            }

            for (size_t vertIdx = 0; vertIdx < mesh.vertices.size(); vertIdx++)
            {
                const auto& vertex = mesh.vertices[vertIdx];

                // Get skeleton bone index for this vertex
                uint32_t groupIdx = (vertIdx < vertexBoneGroups.size()) ? vertexBoneGroups[vertIdx] : 0;
                uint32_t skelBone;

                if (useDirectIndices)
                {
                    skelBone = groupIdx < boneCount ? static_cast<uint32_t>(groupIdx) : 0;
                }
                else
                {
                    skelBone = boneData.MapGroupToSkeletonBone(groupIdx);
                }

                // Clamp to valid bone index
                if (skelBone >= boneCount)
                {
                    skelBone = 0;
                }

                // Accumulate position
                positionSums[skelBone].x += vertex.position.x;
                positionSums[skelBone].y += vertex.position.y;
                positionSums[skelBone].z += vertex.position.z;
                vertexCounts[skelBone]++;
            }
        }

        // Compute centroids
        std::vector<DirectX::XMFLOAT3> meshBindPositions(boneCount);
        for (size_t i = 0; i < boneCount; i++)
        {
            if (vertexCounts[i] > 0)
            {
                float invCount = 1.0f / static_cast<float>(vertexCounts[i]);
                meshBindPositions[i] = {
                    positionSums[i].x * invCount,
                    positionSums[i].y * invCount,
                    positionSums[i].z * invCount
                };
            }
            else
            {
                // No vertices for this bone - use animation bind position as fallback
                if (i < clip->boneTracks.size())
                {
                    meshBindPositions[i] = clip->boneTracks[i].basePosition;
                }
                else
                {
                    meshBindPositions[i] = {0.0f, 0.0f, 0.0f};
                }
            }
        }

        return meshBindPositions;
    }

    /**
     * @brief Applies mesh-derived bind positions to the animation controller.
     *
     * Call this after ComputeMeshBindPositions() to enable accurate skinning.
     */
    void ApplyMeshBindPositions()
    {
        if (controller && hasAnimation)
        {
            auto meshBindPositions = ComputeMeshBindPositions();
            if (!meshBindPositions.empty())
            {
                // Debug: Compare mesh vs animation bind positions for first few bones
                LogBB8Debug("ApplyMeshBindPositions: Comparing bind positions\n");
                for (size_t i = 0; i < std::min(size_t(15), meshBindPositions.size()); i++)
                {
                    const auto& meshBind = meshBindPositions[i];
                    const auto& animBind = (i < clip->boneTracks.size()) ?
                        clip->boneTracks[i].basePosition : DirectX::XMFLOAT3{0,0,0};
                    float dist = std::sqrt(
                        (meshBind.x - animBind.x) * (meshBind.x - animBind.x) +
                        (meshBind.y - animBind.y) * (meshBind.y - animBind.y) +
                        (meshBind.z - animBind.z) * (meshBind.z - animBind.z));
                    char msg[256];
                    sprintf_s(msg, "  Bone %zu: mesh=(%.1f,%.1f,%.1f) anim=(%.1f,%.1f,%.1f) dist=%.1f\n",
                        i, meshBind.x, meshBind.y, meshBind.z,
                        animBind.x, animBind.y, animBind.z, dist);
                    LogBB8Debug(msg);
                }

                controller->SetMeshBindPositions(meshBindPositions);
            }
        }
    }

    /**
     * @brief Creates AnimatedMeshInstance objects for skinned rendering.
     *
     * Call this after both model and animation are loaded.
     *
     * @param device D3D11 device for creating GPU resources
     */
    void CreateAnimatedMeshes(ID3D11Device* device)
    {
        if (!hasAnimation || originalMeshes.empty())
            return;

        animatedMeshes.clear();

        for (size_t i = 0; i < originalMeshes.size(); i++)
        {
            const auto& mesh = originalMeshes[i];

            // Get bone data for this submesh (if available)
            const auto& boneData = (i < submeshBoneData.size()) ?
                submeshBoneData[i] : SubmeshBoneData();

            // Get per-vertex bone groups (if available)
            const auto& vertexBoneGroups = (i < perVertexBoneGroups.size()) ?
                perVertexBoneGroups[i] : std::vector<uint32_t>();

            // Get skeleton bone count for validation
            size_t boneCount = clip ? clip->boneTracks.size() : 256;

            // Debug: Check for out-of-range bone indices
            if (i == 0)  // Log once
            {
                char msg[256];
                sprintf_s(msg, "CreateAnimatedMeshes: Animation has %zu bones\n", boneCount);
                LogBB8Debug(msg);
            }

            // Check max skeleton bone index in this submesh's palette
            uint32_t maxSkelBone = 0;
            for (uint32_t skelBone : boneData.skeletonBoneIndices)
            {
                if (skelBone > maxSkelBone) maxSkelBone = skelBone;
            }
            if (maxSkelBone >= boneCount)
            {
                char msg[256];
                sprintf_s(msg, "  WARNING Submesh %zu: maxSkelBoneIdx(%u) >= animBoneCount(%zu)!\n",
                    i, maxSkelBone, boneCount);
                LogBB8Debug(msg);
            }

            // Create skinned vertices
            auto skinnedVertices = CreateSkinnedVertices(mesh, boneData, vertexBoneGroups, boneCount);

            // Create AnimatedMeshInstance
            auto animMesh = std::make_shared<AnimatedMeshInstance>(
                device, skinnedVertices, mesh.indices, static_cast<int>(i));

            animatedMeshes.push_back(animMesh);
        }

        hasSkinnedMeshes = !animatedMeshes.empty();
    }

    /**
     * @brief Updates bone matrices in all animated meshes.
     *
     * @param context D3D11 device context for updating constant buffers.
     */
    void UpdateAnimatedMeshBones(ID3D11DeviceContext* context)
    {
        if (!hasAnimation || !controller || animatedMeshes.empty() || !context)
            return;

        const auto& boneMatrices = controller->GetBoneMatrices();

        for (auto& animMesh : animatedMeshes)
        {
            if (animMesh)
            {
                animMesh->UpdateBoneMatrices(context, boneMatrices);
            }
        }
    }

    /**
     * @brief Renders all animated meshes with the skinned vertex shader.
     *
     * @param context D3D11 device context.
     * @param lodQuality LOD quality level.
     */
    void RenderAnimatedMeshes(ID3D11DeviceContext* context, LODQuality lodQuality = LODQuality::High)
    {
        if (!hasSkinnedMeshes || animatedMeshes.empty())
            return;

        for (size_t i = 0; i < animatedMeshes.size(); i++)
        {
            // Check submesh visibility
            if (!visualization.showMesh || !visualization.IsSubmeshVisible(i))
                continue;

            auto& animMesh = animatedMeshes[i];
            if (animMesh)
            {
                animMesh->Draw(context, lodQuality);
            }
        }
    }
};

// Global animation state accessible from other modules
extern AnimationPanelState g_animationState;

/**
 * @brief Draws the animation control panel.
 *
 * Provides controls for:
 * - Find animations button (searches DAT for matching animations)
 * - Animation selection from search results
 * - Play/Pause/Stop
 * - Time scrubbing
 * - Sequence selection
 * - Playback speed
 * - Looping options
 *
 * @param dat_managers Map of DAT managers for searching and loading files
 */
void draw_animation_panel(std::map<int, std::unique_ptr<DATManager>>& dat_managers);
