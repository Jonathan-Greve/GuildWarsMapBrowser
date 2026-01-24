#pragma once

#include "Animation/AnimationController.h"
#include "AnimatedMeshInstance.h"
#include "Vertex.h"
#include "Mesh.h"
#include "FFNA_ModelFile_Other.h"  // For LogBB8Debug
#include <DirectXMath.h>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <atomic>
#include <string>
#include <cstring>
#include <cfloat>
#include <cmath>

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
    bool colorByBoneIndex = false; // If true, color vertices by bone index
    bool showRawBoneIndex = true;  // If true, show raw FA0 palette index; if false, show remapped skeleton bone
    bool useMeshBindPositions = false; // If true, use mesh vertex centroids as bind positions instead of animation

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

    // FA1 bind pose data (from model file, more accurate than BB9 hierarchy)
    // When available, these parent indices should override BB9-derived parents
    std::vector<int32_t> fa1BindPoseParents;
    std::vector<DirectX::XMFLOAT3> fa1BindPosePositions;
    bool hasFA1BindPose = false;

    // Model scale factor (computed from mesh bounding box to fit in view)
    // This is the scale that makes the mesh fit into a 10000 unit bounding box
    float meshScale = 1.0f;

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
        std::vector<uint32_t> groupToSkeletonBone;  // Mapping: group index -> skeleton bone

        void BuildGroupMapping()
        {
            groupToSkeletonBone.clear();
            size_t skelIdx = 0;
            for (size_t i = 0; i < groupSizes.size(); i++)
            {
                uint32_t groupSize = groupSizes[i];
                if (skelIdx < skeletonBoneIndices.size() && groupSize > 0)
                {
                    // Use first bone of group (production mode)
                    groupToSkeletonBone.push_back(skeletonBoneIndices[skelIdx]);
                }
                else
                {
                    groupToSkeletonBone.push_back(0);
                }
                skelIdx += groupSize;
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
        meshScale = 1.0f;
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

        // Clear FA1 bind pose data (will be repopulated for FA0/FA1 format models)
        fa1BindPoseParents.clear();
        fa1BindPosePositions.clear();
        hasFA1BindPose = false;

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

        // Debug: Log the group sizes and mapping
        char debugMsg[2048];
        sprintf_s(debugMsg, "\n=== ExtractBoneData: %u groups, %u boneRefs ===\n",
                  boneGroupCount, totalBoneRefs);
        LogBB8Debug(debugMsg);

        // Log group sizes (first 20)
        std::string sizesLog = "  GroupSizes: ";
        for (size_t i = 0; i < boneData.groupSizes.size() && i < 20; i++)
        {
            char buf[16];
            sprintf_s(buf, "%u ", boneData.groupSizes[i]);
            sizesLog += buf;
        }
        if (boneData.groupSizes.size() > 20) sizesLog += "...";
        sizesLog += "\n";
        LogBB8Debug(sizesLog.c_str());

        // Compute sum of group sizes
        uint32_t sumSizes = 0;
        for (uint32_t gs : boneData.groupSizes) sumSizes += gs;
        sprintf_s(debugMsg, "  Sum of groupSizes: %u (should equal totalBoneRefs=%u)\n",
                  sumSizes, totalBoneRefs);
        LogBB8Debug(debugMsg);

        // Log first 20 skeleton bone indices
        std::string skelLog = "  SkeletonBoneIndices: ";
        for (size_t i = 0; i < boneData.skeletonBoneIndices.size() && i < 20; i++)
        {
            char buf[16];
            sprintf_s(buf, "[%zu]=%u ", i, boneData.skeletonBoneIndices[i]);
            skelLog += buf;
        }
        if (boneData.skeletonBoneIndices.size() > 20) skelLog += "...";
        skelLog += "\n";
        LogBB8Debug(skelLog.c_str());

        // Log ALL group->skeleton mappings to find any mapping to bones 0-9
        std::string mapLog = "  GroupToSkeleton: ";
        std::set<uint32_t> uniqueMappedBones;
        uint32_t minMappedBone = UINT32_MAX, maxMappedBone = 0;
        for (size_t i = 0; i < boneData.groupToSkeletonBone.size(); i++)
        {
            uint32_t bone = boneData.groupToSkeletonBone[i];
            uniqueMappedBones.insert(bone);
            if (bone < minMappedBone) minMappedBone = bone;
            if (bone > maxMappedBone) maxMappedBone = bone;
            if (i < 20)
            {
                char buf[16];
                sprintf_s(buf, "[%zu]->%u ", i, bone);
                mapLog += buf;
            }
        }
        if (boneData.groupToSkeletonBone.size() > 20) mapLog += "...";
        mapLog += "\n";
        LogBB8Debug(mapLog.c_str());

        // Log range and any low-numbered bones
        sprintf_s(debugMsg, "  MappedBones: min=%u, max=%u, unique=%zu\n",
                  minMappedBone, maxMappedBone, uniqueMappedBones.size());
        LogBB8Debug(debugMsg);

        // Log any bones < 10 (these shouldn't exist for the pig)
        std::string lowBonesLog;
        for (uint32_t b : uniqueMappedBones)
        {
            if (b < 10)
            {
                char buf[16];
                sprintf_s(buf, "%u ", b);
                lowBonesLog += buf;
            }
        }
        if (!lowBonesLog.empty())
        {
            sprintf_s(debugMsg, "  WARNING: Groups map to low bones (<10): %s\n", lowBonesLog.c_str());
            LogBB8Debug(debugMsg);
        }

        return boneData;
    }

    /**
     * @brief Creates skinned vertices from a mesh and bone data.
     *
     * @param mesh The original mesh with GWVertex
     * @param boneData Bone group mapping for this submesh
     * @param vertexBoneGroups Per-vertex bone group indices from ModelVertex.group
     * @param boneCount Total number of bones in the skeleton (for validation)
     * @param hierarchyMode Hierarchy encoding mode (for bone index adjustment)
     * @param submeshIndex Submesh index for logging
     * @return Vector of SkinnedGWVertex with bone weights set
     */
    static std::vector<SkinnedGWVertex> CreateSkinnedVertices(
        const Mesh& mesh,
        const SubmeshBoneData& boneData,
        const std::vector<uint32_t>& vertexBoneGroups,
        size_t boneCount = 256,
        GW::Animation::HierarchyMode hierarchyMode = GW::Animation::HierarchyMode::TreeDepth,
        size_t submeshIndex = 0)
    {
        (void)hierarchyMode;  // Unused
        (void)submeshIndex;   // Unused

        std::vector<SkinnedGWVertex> skinnedVertices;
        skinnedVertices.reserve(mesh.vertices.size());

        // Determine if we should use direct indices (fallback) or palette mapping
        bool useDirectIndices = false;
        uint32_t maxVertexBoneIdx = 0;
        for (const auto& idx : vertexBoneGroups)
        {
            if (idx > maxVertexBoneIdx) maxVertexBoneIdx = idx;
        }

        // Use palette mapping if available and vertex indices fit within the mapping
        if (boneData.groupToSkeletonBone.empty())
        {
            useDirectIndices = true;
        }
        else if (maxVertexBoneIdx >= boneData.groupToSkeletonBone.size() && maxVertexBoneIdx < boneCount)
        {
            useDirectIndices = true;
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
                // PALETTE mode: Use group-size-skipping to map palette index to skeleton bone
                skelBone = boneData.MapGroupToSkeletonBone(groupIdx);
                if (skelBone >= boneCount)
                {
                    skelBone = skelBone % boneCount;
                }
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
            controller->Initialize(clip);
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

        // Get hierarchy mode from clip (or default to TreeDepth)
        GW::Animation::HierarchyMode hierarchyMode = clip ? clip->hierarchyMode : GW::Animation::HierarchyMode::TreeDepth;

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

            // Create skinned vertices
            auto skinnedVertices = CreateSkinnedVertices(mesh, boneData, vertexBoneGroups, boneCount, hierarchyMode, i);

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
     * Uses direct mapping: skeleton bone X = animation bone X.
     *
     * @param context D3D11 device context for updating constant buffers.
     */
    void UpdateAnimatedMeshBones(ID3D11DeviceContext* context)
    {
        if (!hasAnimation || !controller || animatedMeshes.empty() || !context)
            return;

        const auto& animBoneMatrices = controller->GetBoneMatrices();

        // Direct mapping: skeleton bone X = animation bone X
        for (auto& animMesh : animatedMeshes)
        {
            if (animMesh)
            {
                animMesh->UpdateBoneMatrices(context, animBoneMatrices);
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
 * @brief Sets the DAT managers pointer for animation loading.
 * Must be called before AutoLoadAnimation can work.
 */
void SetAnimationDATManagers(std::map<int, std::unique_ptr<DATManager>>* dat_managers);

/**
 * @brief Automatically loads animation for the current model.
 *
 * First tries to load animation from the same file as the model.
 * If not found, searches for files with matching model hashes and loads the first one.
 *
 * @param dat_managers Map of DAT managers to search
 */
void AutoLoadAnimation(std::map<int, std::unique_ptr<DATManager>>& dat_managers);

/**
 * @brief Automatically loads animation using the stored DAT managers pointer.
 * SetAnimationDATManagers must be called first.
 */
void AutoLoadAnimationFromStoredManagers();

/**
 * @brief Starts a background search for animations matching the current model.
 *
 * Unlike AutoLoadAnimation, this function will search even if an animation
 * is already loaded. Results are stored in g_animationState.searchResults.
 *
 * @param dat_managers Map of DAT managers to search
 */
void StartAnimationSearch(std::map<int, std::unique_ptr<DATManager>>& dat_managers);

/**
 * @brief Loads an animation from the search results by index.
 *
 * @param resultIndex Index into g_animationState.searchResults
 * @param dat_managers Map of DAT managers for loading
 */
void LoadAnimationFromSearchResult(int resultIndex, std::map<int, std::unique_ptr<DATManager>>& dat_managers);
