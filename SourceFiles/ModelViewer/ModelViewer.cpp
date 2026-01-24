#include "pch.h"
#include "ModelViewer.h"
#include "animation_state.h"
#include "MapRenderer.h"

using namespace DirectX;

// Global model viewer state instance
ModelViewerState g_modelViewerState;

void ActivateModelViewer(MapRenderer* mapRenderer)
{
    if (!mapRenderer)
        return;

    // Copy mesh data from animation state
    g_modelViewerState.meshes = g_animationState.originalMeshes;
    g_modelViewerState.meshIds = g_animationState.meshIds;
    g_modelViewerState.modelFileId = g_animationState.currentFileId;

    // Copy bone group data for vertex counting
    g_modelViewerState.vertexBoneGroups = g_animationState.perVertexBoneGroups;

    // Share animation controller and clip
    g_modelViewerState.animController = g_animationState.controller;
    g_modelViewerState.animClip = g_animationState.clip;

    // Compute bounding box
    g_modelViewerState.ComputeBounds();

    // Update bone display info
    g_modelViewerState.UpdateBoneInfo();

    // Store map state for restoration (if terrain was loaded)
    g_modelViewerState.hadMapLoaded = (mapRenderer->GetTerrain() != nullptr);
    g_modelViewerState.previousTerrainId = mapRenderer->GetTerrainMeshId();

    // Hide terrain/map meshes when in model viewer mode
    if (g_modelViewerState.hadMapLoaded)
    {
        int terrainId = mapRenderer->GetTerrainMeshId();
        if (terrainId >= 0)
        {
            mapRenderer->SetMeshShouldRender(terrainId, false);
        }

        // Hide water
        int waterId = mapRenderer->GetWaterMeshId();
        if (waterId >= 0)
        {
            mapRenderer->SetMeshShouldRender(waterId, false);
        }
    }

    g_modelViewerState.isActive = true;
    // Defer camera fit to first frame when viewport is properly set
    g_modelViewerState.needsCameraFit = true;

    // Enable bone visualization by default in model viewer
    g_animationState.visualization.showBones = true;
    g_modelViewerState.options.showBones = true;
}

void DeactivateModelViewer(MapRenderer* mapRenderer)
{
    if (!mapRenderer)
        return;

    // Hide the model meshes that were being viewed
    for (int meshId : g_modelViewerState.meshIds)
    {
        if (meshId >= 0)
        {
            mapRenderer->SetMeshShouldRender(meshId, false);
        }
    }

    // Also hide animation state meshes (skinned meshes)
    for (int meshId : g_animationState.meshIds)
    {
        if (meshId >= 0)
        {
            mapRenderer->SetMeshShouldRender(meshId, false);
        }
    }

    // Restore terrain/map visibility
    if (g_modelViewerState.hadMapLoaded)
    {
        if (g_modelViewerState.previousTerrainId >= 0)
        {
            mapRenderer->SetMeshShouldRender(g_modelViewerState.previousTerrainId, true);
        }

        int waterId = mapRenderer->GetWaterMeshId();
        if (waterId >= 0)
        {
            mapRenderer->SetMeshShouldRender(waterId, true);
        }
    }

    // Clear camera override so map camera takes over
    mapRenderer->ClearCameraOverride();

    // Reset model viewer state
    g_modelViewerState.isActive = false;
    g_modelViewerState.needsCameraFit = false;
    g_modelViewerState.meshes.clear();
    g_modelViewerState.meshIds.clear();
    g_modelViewerState.bones.clear();
    g_modelViewerState.modelFileId = 0;
    g_modelViewerState.camera->Reset();

    // Disable bone visualization when leaving model viewer
    g_animationState.visualization.showBones = false;
}

void UpdateModelViewer(float deltaTime)
{
    if (!g_modelViewerState.isActive)
        return;

    // Update camera
    g_modelViewerState.camera->Update(deltaTime);

    // Sync bone info from animation state if needed
    if (g_animationState.controller && g_animationState.clip)
    {
        // Update bone positions from animation controller
        const auto& bonePositions = g_animationState.controller->GetBoneWorldPositions();

        // Resize bones array if needed
        if (g_modelViewerState.bones.size() != bonePositions.size())
        {
            g_modelViewerState.animClip = g_animationState.clip;
            g_modelViewerState.animController = g_animationState.controller;
            g_modelViewerState.UpdateBoneInfo();
        }

        // Get transformation matrices (same as MapBrowser.cpp bone visualization)
        XMMATRIX worldMatrix = XMMatrixIdentity();
        if (!g_animationState.perMeshPerObjectCB.empty())
        {
            worldMatrix = XMLoadFloat4x4(&g_animationState.perMeshPerObjectCB[0].world);
        }
        XMMATRIX yRotation = XMMatrixRotationY(-XM_PIDIV2);
        XMMATRIX combinedMatrix = yRotation * worldMatrix;

        // Update positions with transformation applied
        for (size_t i = 0; i < bonePositions.size() && i < g_modelViewerState.bones.size(); i++)
        {
            XMVECTOR bonePos = XMVectorSet(bonePositions[i].x, bonePositions[i].y, bonePositions[i].z, 1.0f);
            XMVECTOR transformedPos = XMVector3Transform(bonePos, combinedMatrix);
            XMStoreFloat3(&g_modelViewerState.bones[i].position, transformedPos);
        }
    }

    // Auto-fit camera if needed
    if (g_modelViewerState.needsCameraFit)
    {
        g_modelViewerState.camera->FitToBounds(
            g_modelViewerState.boundsMin,
            g_modelViewerState.boundsMax);
        g_modelViewerState.needsCameraFit = false;
    }
}

void HandleModelViewerInput(float deltaX, float deltaY, bool leftButton, bool rightButton, float scrollDelta)
{
    if (!g_modelViewerState.isActive)
        return;

    auto* camera = g_modelViewerState.camera.get();
    if (!camera)
        return;

    // Update drag mode based on button state
    if (leftButton && !rightButton)
    {
        camera->SetDragMode(1);  // Orbit
        camera->OnOrbitDrag(deltaX, deltaY);
    }
    else if (rightButton && !leftButton)
    {
        camera->SetDragMode(2);  // Pan
        camera->OnPanDrag(deltaX, deltaY);
    }
    else
    {
        camera->SetDragMode(0);  // None
    }

    // Handle zoom
    if (scrollDelta != 0.0f)
    {
        camera->OnZoom(scrollDelta);
    }
}

int PickBoneAtScreenPos(float screenX, float screenY, float screenWidth, float screenHeight)
{
    if (!g_modelViewerState.isActive)
        return -1;

    auto* camera = g_modelViewerState.camera.get();
    if (!camera)
        return -1;

    // Get bone positions - prefer from animation state if available
    std::vector<XMFLOAT3> bonePositions;
    if (g_animationState.controller && g_animationState.clip)
    {
        bonePositions = g_animationState.controller->GetBoneWorldPositions();
    }
    else if (!g_modelViewerState.bones.empty())
    {
        for (const auto& bone : g_modelViewerState.bones)
        {
            bonePositions.push_back(bone.position);
        }
    }

    if (bonePositions.empty())
        return -1;

    // Transform bone positions by the same world matrix used for rendering
    // (same as MapBrowser.cpp bone visualization code)
    XMMATRIX worldMatrix = XMMatrixIdentity();
    if (!g_animationState.perMeshPerObjectCB.empty())
    {
        worldMatrix = XMLoadFloat4x4(&g_animationState.perMeshPerObjectCB[0].world);
    }

    // Apply -90 degree Y rotation to align animation skeleton with mesh
    XMMATRIX yRotation = XMMatrixRotationY(-XM_PIDIV2);
    XMMATRIX combinedMatrix = yRotation * worldMatrix;

    // Transform all bone positions
    for (auto& pos : bonePositions)
    {
        XMVECTOR bonePos = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);
        XMVECTOR transformedPos = XMVector3Transform(bonePos, combinedMatrix);
        XMStoreFloat3(&pos, transformedPos);
    }

    // Convert screen coords to normalized device coords
    float ndcX = (2.0f * screenX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * screenY / screenHeight);

    // Get view and projection matrices
    XMMATRIX view = camera->GetView();
    XMMATRIX proj = camera->GetProj();
    XMMATRIX viewProj = view * proj;

    // Create ray from camera through screen point
    XMVECTOR rayOrigin = camera->GetPositionV();

    // Unproject near and far points
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);
    XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProj);
    XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProj);
    XMVECTOR rayDir = XMVector3Normalize(farPoint - nearPoint);

    // Use the same joint radius as the bone visualization for picking
    // The visualization uses g_animationState.visualization.jointRadius (default 50)
    float jointRadius = g_animationState.visualization.jointRadius;

    // Add extra tolerance for easier picking (1.5x the visual radius)
    float pickRadius = jointRadius * 1.5f;

    // Find closest bone to ray
    float closestDist = FLT_MAX;
    int closestBone = -1;

    for (size_t i = 0; i < bonePositions.size(); i++)
    {
        const auto& pos = bonePositions[i];
        XMVECTOR bonePos = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);

        // Ray-sphere intersection: find closest point on ray to bone
        XMVECTOR toBone = bonePos - rayOrigin;
        float t = XMVectorGetX(XMVector3Dot(toBone, rayDir));

        if (t < 0)
            continue;  // Bone is behind camera

        XMVECTOR closestPointOnRay = rayOrigin + rayDir * t;
        float distFromRay = XMVectorGetX(XMVector3Length(closestPointOnRay - bonePos));

        if (distFromRay < pickRadius && t < closestDist)
        {
            closestDist = t;
            closestBone = static_cast<int>(i);
        }
    }

    return closestBone;
}
