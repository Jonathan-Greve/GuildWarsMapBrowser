#include "pch.h"
#include "OrbitalCamera.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

OrbitalCamera::OrbitalCamera()
    : m_target{ 0.0f, 0.0f, 0.0f }
    , m_distance(1000.0f)
    , m_yaw(0.0f)
    , m_pitch(XM_PIDIV4)  // Start at 45 degrees down
    , m_fovY(60.0f * XM_PI / 180.0f)
    , m_aspectRatio(16.0f / 9.0f)
    , m_nearZ(1.0f)
    , m_farZ(200000.0f)
    , m_viewDirty(true)
    , m_dragMode(0)
{
    XMStoreFloat4x4(&m_view, XMMatrixIdentity());
    XMStoreFloat4x4(&m_proj, XMMatrixIdentity());
    UpdateViewMatrix();
    SetPerspective(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
}

void OrbitalCamera::Update(float dt)
{
    if (m_viewDirty)
    {
        UpdateViewMatrix();
        m_viewDirty = false;
    }
}

void OrbitalCamera::Reset()
{
    m_target = { 0.0f, 0.0f, 0.0f };
    m_distance = 1000.0f;
    m_yaw = 0.0f;
    m_pitch = XM_PIDIV4;
    m_viewDirty = true;
}

void OrbitalCamera::SetTarget(const XMFLOAT3& target)
{
    m_target = target;
    m_viewDirty = true;
}

void OrbitalCamera::SetDistance(float distance)
{
    m_distance = std::clamp(distance, m_minDistance, m_maxDistance);
    m_viewDirty = true;
}

void OrbitalCamera::FitToBounds(const XMFLOAT3& boundsMin, const XMFLOAT3& boundsMax)
{
    // Calculate center of bounds
    m_target.x = (boundsMin.x + boundsMax.x) * 0.5f;
    m_target.y = (boundsMin.y + boundsMax.y) * 0.5f;
    m_target.z = (boundsMin.z + boundsMax.z) * 0.5f;

    // Calculate bounding sphere radius
    float dx = boundsMax.x - boundsMin.x;
    float dy = boundsMax.y - boundsMin.y;
    float dz = boundsMax.z - boundsMin.z;
    float radius = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;

    // Ensure minimum radius for very small models
    radius = std::max(radius, 10.0f);

    // Calculate distance needed to fit the model in view
    // Use tan(halfFov) to ensure the entire sphere is visible
    float halfFovY = m_fovY * 0.5f;
    float halfFovX = std::atan(std::tan(halfFovY) * m_aspectRatio);
    float halfFov = std::min(halfFovY, halfFovX);

    // Add padding (2.0x) for comfortable viewing
    m_distance = (radius / std::tan(halfFov)) * 2.0f;
    m_distance = std::clamp(m_distance, m_minDistance, m_maxDistance);

    // Reset orbit angles to a nice viewing angle
    m_yaw = XM_PIDIV4;        // 45 degrees around
    m_pitch = XM_PIDIV4;      // 45 degrees down

    m_viewDirty = true;
}

void OrbitalCamera::SetPerspective(float fovY, float aspect, float nearZ, float farZ)
{
    m_fovY = fovY;
    m_aspectRatio = aspect;
    m_nearZ = nearZ;
    m_farZ = farZ;
    XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ));
}

void OrbitalCamera::OnViewportChanged(float width, float height)
{
    m_aspectRatio = width / height;
    SetPerspective(m_fovY, m_aspectRatio, m_nearZ, m_farZ);
}

void OrbitalCamera::OnOrbitDrag(float deltaX, float deltaY)
{
    // Dragging left/right rotates camera around model horizontally
    // Dragging up/down rotates camera around model vertically
    m_yaw += deltaX * m_orbitSensitivity;
    m_pitch += deltaY * m_orbitSensitivity;

    // Clamp pitch to avoid flipping
    m_pitch = std::clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

    m_viewDirty = true;
}

void OrbitalCamera::OnPanDrag(float deltaX, float deltaY)
{
    // Calculate pan amount based on distance (pan faster when zoomed out)
    float panScale = m_distance * m_panSensitivity * 0.001f;

    // Get right and up vectors for panning in camera space
    XMFLOAT3 right = GetRight();
    XMFLOAT3 up = GetUp();

    m_target.x -= right.x * deltaX * panScale - up.x * deltaY * panScale;
    m_target.y -= right.y * deltaX * panScale - up.y * deltaY * panScale;
    m_target.z -= right.z * deltaX * panScale - up.z * deltaY * panScale;

    m_viewDirty = true;
}

void OrbitalCamera::OnZoom(float delta)
{
    // Exponential zoom for smooth feel
    float zoomFactor = 1.0f - delta * m_zoomSensitivity;
    m_distance *= zoomFactor;
    m_distance = std::clamp(m_distance, m_minDistance, m_maxDistance);
    m_viewDirty = true;
}

void OrbitalCamera::OnMouseMove(float yaw_angle_radians, float pitch_angle_radians)
{
    // This is called by InputManager with the delta mouse movement
    // We need to know the drag mode to determine what to do
    if (m_dragMode == 1)
    {
        // Orbit mode - convert radians to pixel-like delta
        OnOrbitDrag(yaw_angle_radians * 200.0f, pitch_angle_radians * 200.0f);
    }
    else if (m_dragMode == 2)
    {
        // Pan mode
        OnPanDrag(yaw_angle_radians * 200.0f, pitch_angle_radians * 200.0f);
    }
}

void OrbitalCamera::UpdateViewMatrix()
{
    // Calculate camera position from orbital parameters
    // Position = Target + spherical coords
    float cosPitch = std::cos(m_pitch);
    float sinPitch = std::sin(m_pitch);
    float cosYaw = std::cos(m_yaw);
    float sinYaw = std::sin(m_yaw);

    XMFLOAT3 offset;
    offset.x = m_distance * cosPitch * sinYaw;
    offset.y = m_distance * sinPitch;
    offset.z = m_distance * cosPitch * cosYaw;

    XMFLOAT3 pos;
    pos.x = m_target.x + offset.x;
    pos.y = m_target.y + offset.y;
    pos.z = m_target.z + offset.z;

    XMVECTOR eyePos = XMLoadFloat3(&pos);
    XMVECTOR focusPos = XMLoadFloat3(&m_target);
    XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMStoreFloat4x4(&m_view, XMMatrixLookAtLH(eyePos, focusPos, upDir));
}

XMMATRIX OrbitalCamera::GetView() const
{
    return XMLoadFloat4x4(&m_view);
}

XMMATRIX OrbitalCamera::GetProj() const
{
    return XMLoadFloat4x4(&m_proj);
}

XMFLOAT4X4 OrbitalCamera::GetView4x4() const
{
    return m_view;
}

XMFLOAT4X4 OrbitalCamera::GetProj4x4() const
{
    return m_proj;
}

XMFLOAT3 OrbitalCamera::GetPosition() const
{
    float cosPitch = std::cos(m_pitch);
    float sinPitch = std::sin(m_pitch);
    float cosYaw = std::cos(m_yaw);
    float sinYaw = std::sin(m_yaw);

    return {
        m_target.x + m_distance * cosPitch * sinYaw,
        m_target.y + m_distance * sinPitch,
        m_target.z + m_distance * cosPitch * cosYaw
    };
}

XMVECTOR OrbitalCamera::GetPositionV() const
{
    XMFLOAT3 pos = GetPosition();
    return XMLoadFloat3(&pos);
}

XMFLOAT3 OrbitalCamera::GetForward() const
{
    XMFLOAT3 pos = GetPosition();
    XMFLOAT3 forward = {
        m_target.x - pos.x,
        m_target.y - pos.y,
        m_target.z - pos.z
    };

    // Normalize
    float len = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (len > 0.0001f)
    {
        forward.x /= len;
        forward.y /= len;
        forward.z /= len;
    }
    return forward;
}

XMFLOAT3 OrbitalCamera::GetRight() const
{
    XMFLOAT3 forward = GetForward();
    XMVECTOR forwardV = XMLoadFloat3(&forward);
    XMVECTOR upV = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR rightV = XMVector3Cross(upV, forwardV);
    rightV = XMVector3Normalize(rightV);

    XMFLOAT3 right;
    XMStoreFloat3(&right, rightV);
    return right;
}

XMFLOAT3 OrbitalCamera::GetUp() const
{
    XMFLOAT3 forward = GetForward();
    XMFLOAT3 right = GetRight();

    XMVECTOR forwardV = XMLoadFloat3(&forward);
    XMVECTOR rightV = XMLoadFloat3(&right);
    XMVECTOR upV = XMVector3Cross(forwardV, rightV);
    upV = XMVector3Normalize(upV);

    XMFLOAT3 up;
    XMStoreFloat3(&up, upV);
    return up;
}
