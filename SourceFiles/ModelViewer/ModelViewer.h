#pragma once

#include "OrbitalCamera.h"
#include "Mesh.h"
#include "Animation/AnimationClip.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationController.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <vector>
#include <string>

// Forward declarations
class MapRenderer;
class DATManager;
class MeshManager;
class TextureManager;

/**
 * @brief Bone information for display in the model viewer.
 */
struct BoneDisplayInfo
{
    int index = -1;
    int parentIndex = -1;
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    int vertexCount = 0;  // Number of vertices weighted to this bone
    bool isSelected = false;
};

/**
 * @brief Visualization options for the model viewer.
 */
struct ModelViewerOptions
{
    bool showMesh = true;
    bool showWireframe = false;
    bool showBones = true;
    bool showBoneLabels = true;

    // Background color
    DirectX::XMFLOAT4 backgroundColor = { 0.1f, 0.1f, 0.15f, 1.0f };

    // Bone visualization
    float boneRadius = 20.0f;
    DirectX::XMFLOAT4 boneColor = { 1.0f, 1.0f, 0.0f, 1.0f };      // Yellow
    DirectX::XMFLOAT4 selectedBoneColor = { 0.0f, 1.0f, 0.0f, 1.0f }; // Green
    DirectX::XMFLOAT4 boneLineColor = { 0.8f, 0.8f, 0.0f, 1.0f };   // Pale yellow

    // Selected bone highlighting
    int selectedBoneIndex = -1;
    bool highlightAffectedVertices = true;
    DirectX::XMFLOAT4 highlightColor = { 1.0f, 0.5f, 0.0f, 1.0f };  // Orange
};

/**
 * @brief Global state for the model viewer scene.
 */
struct ModelViewerState
{
    bool isActive = false;              // Whether model viewer scene is active
    bool needsCameraFit = false;        // True when camera should auto-fit to new model

    // Model data
    std::vector<Mesh> meshes;
    std::vector<int> meshIds;           // IDs from MapRenderer
    uint32_t modelFileId = 0;
    int modelMftIndex = -1;             // MFT index for saving
    DATManager* modelDatManager = nullptr;  // DAT manager for saving model

    // Animation data (for saving)
    uint32_t animFileId = 0;            // Currently loaded animation file ID
    int animMftIndex = -1;              // Animation MFT index
    DATManager* animDatManager = nullptr;   // DAT manager for saving animation

    // Bounding box
    DirectX::XMFLOAT3 boundsMin = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 boundsMax = { 0.0f, 0.0f, 0.0f };

    // Bone data (from animation panel state if available)
    std::vector<BoneDisplayInfo> bones;
    std::vector<int32_t> boneParents;

    // Animation controller reference (shared with animation panel)
    std::shared_ptr<GW::Animation::AnimationController> animController;
    std::shared_ptr<GW::Animation::AnimationClip> animClip;

    // Per-vertex bone assignments (indexed by mesh, then by vertex)
    std::vector<std::vector<uint32_t>> vertexBoneGroups;

    // Visualization options
    ModelViewerOptions options;

    // Orbital camera
    std::unique_ptr<OrbitalCamera> camera;

    // Previous map state (for restoring when exiting model viewer)
    bool hadMapLoaded = false;
    int previousTerrainId = -1;

    ModelViewerState()
    {
        camera = std::make_unique<OrbitalCamera>();
    }

    void Reset()
    {
        isActive = false;
        needsCameraFit = false;
        meshes.clear();
        meshIds.clear();
        modelFileId = 0;
        modelMftIndex = -1;
        modelDatManager = nullptr;
        animFileId = 0;
        animMftIndex = -1;
        animDatManager = nullptr;
        boundsMin = { 0.0f, 0.0f, 0.0f };
        boundsMax = { 0.0f, 0.0f, 0.0f };
        bones.clear();
        boneParents.clear();
        animController.reset();
        animClip.reset();
        vertexBoneGroups.clear();
        options = ModelViewerOptions();
        camera->Reset();
    }

    /**
     * @brief Computes bounding box from all loaded meshes.
     *
     * The renderer transforms meshes to fit in a 10000 unit bounding box,
     * centered at origin in X/Z, with Y positioned so the bottom is at 0.
     * This function computes bounds that match the rendered position.
     */
    void ComputeBounds()
    {
        if (meshes.empty())
            return;

        // First compute original bounds
        float origMinX = FLT_MAX, origMinY = FLT_MAX, origMinZ = FLT_MAX;
        float origMaxX = -FLT_MAX, origMaxY = -FLT_MAX, origMaxZ = -FLT_MAX;

        for (const auto& mesh : meshes)
        {
            for (const auto& vertex : mesh.vertices)
            {
                origMinX = std::min(origMinX, vertex.position.x);
                origMinY = std::min(origMinY, vertex.position.y);
                origMinZ = std::min(origMinZ, vertex.position.z);

                origMaxX = std::max(origMaxX, vertex.position.x);
                origMaxY = std::max(origMaxY, vertex.position.y);
                origMaxZ = std::max(origMaxZ, vertex.position.z);
            }
        }

        // Calculate dimensions and scale (same as draw_dat_browser.cpp)
        float modelWidth = origMaxX - origMinX;
        float modelHeight = origMaxY - origMinY;
        float modelDepth = origMaxZ - origMinZ;
        float maxDimension = std::max({ modelWidth, modelHeight, modelDepth });

        float scale = (maxDimension > 0.001f) ? (10000.0f / maxDimension) : 1.0f;

        // The renderer applies: scale first, then translate
        // Translation is: (-centerX * scale, (-centerY + modelHeight/2) * scale, -centerZ * scale)
        // This positions: X centered at 0, Z centered at 0, Y with bottom at 0

        // After transformation:
        // - X range: [-modelWidth * scale / 2, +modelWidth * scale / 2]
        // - Y range: [0, modelHeight * scale]
        // - Z range: [-modelDepth * scale / 2, +modelDepth * scale / 2]

        float scaledWidth = modelWidth * scale;
        float scaledHeight = modelHeight * scale;
        float scaledDepth = modelDepth * scale;

        boundsMin.x = -scaledWidth * 0.5f;
        boundsMin.y = 0.0f;
        boundsMin.z = -scaledDepth * 0.5f;

        boundsMax.x = scaledWidth * 0.5f;
        boundsMax.y = scaledHeight;
        boundsMax.z = scaledDepth * 0.5f;
    }

    /**
     * @brief Gets model center from bounding box.
     */
    DirectX::XMFLOAT3 GetCenter() const
    {
        return {
            (boundsMin.x + boundsMax.x) * 0.5f,
            (boundsMin.y + boundsMax.y) * 0.5f,
            (boundsMin.z + boundsMax.z) * 0.5f
        };
    }

    /**
     * @brief Gets model radius (half of diagonal).
     */
    float GetRadius() const
    {
        float dx = boundsMax.x - boundsMin.x;
        float dy = boundsMax.y - boundsMin.y;
        float dz = boundsMax.z - boundsMin.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;
    }

    /**
     * @brief Updates bone display info from animation data.
     */
    void UpdateBoneInfo()
    {
        bones.clear();

        // Try to get bone data from animation clip
        if (animClip && !animClip->boneTracks.empty())
        {
            bones.resize(animClip->boneTracks.size());
            boneParents = animClip->boneParents;

            for (size_t i = 0; i < animClip->boneTracks.size(); i++)
            {
                bones[i].index = static_cast<int>(i);
                bones[i].parentIndex = i < boneParents.size() ? boneParents[i] : -1;
                bones[i].position = animClip->boneTracks[i].basePosition;
                bones[i].vertexCount = 0;
                bones[i].isSelected = (static_cast<int>(i) == options.selectedBoneIndex);
            }

            // Count vertices per bone
            ComputeVertexCountsPerBone();
        }
    }

    /**
     * @brief Counts how many vertices are weighted to each bone.
     */
    void ComputeVertexCountsPerBone()
    {
        if (bones.empty() || vertexBoneGroups.empty())
            return;

        // Reset counts
        for (auto& bone : bones)
        {
            bone.vertexCount = 0;
        }

        // Count vertices per mesh
        for (size_t meshIdx = 0; meshIdx < vertexBoneGroups.size() && meshIdx < meshes.size(); meshIdx++)
        {
            const auto& boneGroups = vertexBoneGroups[meshIdx];
            for (size_t vertIdx = 0; vertIdx < boneGroups.size(); vertIdx++)
            {
                uint32_t boneIdx = boneGroups[vertIdx];
                if (boneIdx < bones.size())
                {
                    bones[boneIdx].vertexCount++;
                }
            }
        }
    }

    /**
     * @brief Selects a bone by index.
     */
    void SelectBone(int boneIndex)
    {
        // Deselect all
        for (auto& bone : bones)
        {
            bone.isSelected = false;
        }

        options.selectedBoneIndex = boneIndex;

        if (boneIndex >= 0 && boneIndex < static_cast<int>(bones.size()))
        {
            bones[boneIndex].isSelected = true;
        }
    }
};

// Global model viewer state
extern ModelViewerState g_modelViewerState;

/**
 * @brief Activates the model viewer for the current model in animation state.
 *
 * Called when user wants to switch to model viewer mode.
 * Copies necessary data from g_animationState.
 *
 * @param mapRenderer The MapRenderer for managing mesh visibility
 */
void ActivateModelViewer(MapRenderer* mapRenderer);

/**
 * @brief Deactivates the model viewer and restores normal map viewing.
 *
 * @param mapRenderer The MapRenderer
 */
void DeactivateModelViewer(MapRenderer* mapRenderer);

/**
 * @brief Updates the model viewer camera and animation.
 *
 * @param deltaTime Time since last frame in seconds
 */
void UpdateModelViewer(float deltaTime);

/**
 * @brief Handles mouse input for the model viewer.
 *
 * @param deltaX Mouse X movement
 * @param deltaY Mouse Y movement
 * @param leftButton Left mouse button state
 * @param rightButton Right mouse button state
 * @param scrollDelta Mouse wheel delta
 */
void HandleModelViewerInput(float deltaX, float deltaY, bool leftButton, bool rightButton, float scrollDelta);

/**
 * @brief Performs bone picking at screen coordinates.
 *
 * @param screenX Screen X coordinate
 * @param screenY Screen Y coordinate
 * @param screenWidth Screen width
 * @param screenHeight Screen height
 * @return Index of picked bone, or -1 if none
 */
int PickBoneAtScreenPos(float screenX, float screenY, float screenWidth, float screenHeight);
