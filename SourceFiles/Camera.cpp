#include "pch.h"
#include "Camera.h"

Camera::Camera() { }

Camera::~Camera() { }

void Camera::Update(float dt) { UpdateViewMatrix(); }

XMVECTOR Camera::GetPosition() const { return XMLoadFloat3(&m_position); }

XMFLOAT3 Camera::GetPosition3f() const { return m_position; }

void Camera::SetPosition(float x, float y, float z)
{
    m_position = XMFLOAT3(x, y, z);
    m_view_should_update = true;
}

XMVECTOR Camera::GetRight() const { return XMLoadFloat3(&m_right); }

XMFLOAT3 Camera::GetRight3f() const { return m_right; }

XMVECTOR Camera::GetUp() const { return XMLoadFloat3(&m_up); }

XMFLOAT3 Camera::GetUp3f() const { return m_up; }

XMVECTOR Camera::GetLook() const { return XMLoadFloat3(&m_look); }

XMFLOAT3 Camera::GetLook3f() const { return m_look; }

void Camera::SetFrustumAsPerspective(float fovY, float aspectRatio, float zNear, float zFar)
{
    XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(fovY, aspectRatio, zNear, zFar));
    m_fov = fovY;
    m_aspectRatio = aspectRatio;
    m_nearZ = zNear;
    m_farZ = zFar;

    m_camera_type = CameraType::Perspective;
}

void Camera::SetFrustumAsOrthographic(float view_width, float view_height, float zn, float zf)
{
    view_width = std::max(1.0f, view_width);
    view_height = std::max(1.0f, view_height);

    XMStoreFloat4x4(&m_proj, XMMatrixOrthographicLH(view_width, view_height, zn, zf));
    m_viewWidth = view_width;
    m_viewHeight = view_height;

    m_nearZ = zn;
    m_farZ = zf;

    m_camera_type = CameraType::Orthographic;
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
    XMVECTOR look = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, look));
    if (XMVector3Equal(right, g_XMZero))
    {
        right = {1, 0, 0, 0};
    }
    XMVECTOR up = XMVector3Cross(right, look);

    XMStoreFloat3(&m_look, look);
    XMStoreFloat3(&m_right, right);
    XMStoreFloat3(&m_up, up);
    XMStoreFloat3(&m_position, pos);
    m_view_should_update = true;

    // Calculate yaw and pitch based on the look vector
    float lookX = XMVectorGetX(look);
    float lookY = XMVectorGetY(look);
    float lookZ = XMVectorGetZ(look);

    m_yaw = atan2f(lookX, lookZ);
    m_pitch = atan2f(lookY, sqrtf(lookX * lookX + lookZ * lookZ));
}

void Camera::SetOrientation(float pitch, float yaw)
{
    // Update pitch and yaw
    m_pitch = pitch;
    m_yaw = yaw;

    // Clamp the pitch to avoid gimbal lock
    const float maxPitch = XM_PI / 2.0f - 0.001;
    m_pitch = std::max(-maxPitch, std::min(maxPitch, m_pitch));

    // Compute the look vector from pitch and yaw
    XMVECTOR look =
      XMVectorSet(cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch), cosf(m_pitch) * cosf(m_yaw), 0.0f);

    // Compute the right and up vectors
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, look));
    XMVECTOR up = XMVector3Cross(look, right);

    // Call LookAt with the computed vectors
    LookAt(XMLoadFloat3(&m_position), XMVectorAdd(XMLoadFloat3(&m_position), look), up);
}

void Camera::Strafe(StrafeDirection strafe_direction, double dt)
{
    float velocity = m_strafe_speed;
    if (strafe_direction == StrafeDirection::Left)
    {
        velocity = -m_strafe_speed;
    }
    XMVECTOR dVec = XMVectorReplicate(velocity);
    XMVECTOR newPos = XMVectorMultiplyAdd(XMLoadFloat3(&m_right), dVec, XMLoadFloat3(&m_position));
    XMStoreFloat3(&m_position, newPos);
    m_view_should_update = true;
}

void Camera::Walk(WalkDirection walk_direction, double dt)
{
    float velocity = m_walk_speed;
    if (walk_direction == WalkDirection::Backward)
    {
        velocity = -m_walk_speed;
    }
    XMVECTOR dVec = XMVectorReplicate(velocity);
    XMVECTOR newPos = XMVectorMultiplyAdd(XMLoadFloat3(&m_look), dVec, XMLoadFloat3(&m_position));

    XMStoreFloat3(&m_position, newPos);
    m_view_should_update = true;
}

void Camera::Pitch(float angle)
{
    auto rotationMatrix = XMMatrixRotationAxis(XMLoadFloat3(&m_right), angle);
    XMVECTOR up = XMVector3TransformNormal(XMLoadFloat3(&m_up), rotationMatrix);
    XMVECTOR look = XMVector3TransformNormal(XMLoadFloat3(&m_look), rotationMatrix);
    XMStoreFloat3(&m_up, up);
    XMStoreFloat3(&m_look, look);

    m_yaw = atan2f(m_look.x, m_look.z);
    m_pitch = atan2f(m_look.y, sqrtf(m_look.x * m_look.x + m_look.z * m_look.z));

    m_view_should_update = true;
}

void Camera::Yaw(float angle)
{
    auto rotationMatrix = XMMatrixRotationY(angle);
    XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rotationMatrix));
    XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), rotationMatrix));
    XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), rotationMatrix));

    m_yaw = atan2f(m_look.x, m_look.z);
    m_pitch = atan2f(m_look.y, sqrtf(m_look.x * m_look.x + m_look.z * m_look.z));

    m_view_should_update = true;
}

XMMATRIX Camera::GetView() const { return XMLoadFloat4x4(&m_view); }

XMFLOAT4X4 Camera::GetView4x4() const { return m_view; }

XMMATRIX Camera::GetProj() const { return XMLoadFloat4x4(&m_proj); }

XMFLOAT4X4 Camera::GetProj4x4() const { return m_proj; }

void Camera::OnViewPortChanged(const float viewport_width, const float viewport_height)
{
    m_aspectRatio = viewport_width / viewport_height;
    if (m_camera_type == CameraType::Perspective)
        XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, m_nearZ, m_farZ));
    else
        XMStoreFloat4x4(&m_proj, XMMatrixOrthographicLH(m_viewWidth, m_viewHeight, m_nearZ, m_farZ));
    m_view_should_update = true;
}

float Camera::GetAspectRatio() const { return m_aspectRatio; }

float Camera::GetFovY() const { return m_fov; }

float Camera::GetViewWidth() const { return m_viewWidth; }

float Camera::GetViewHeight() const { return m_viewHeight; }

float Camera::GetNearZ() const { return m_nearZ; }

float Camera::GetFarZ() const { return m_farZ; }

CameraType Camera::GetCameraType() const { return m_camera_type; }

float Camera::GetYaw() const { return m_yaw; }

float Camera::GetPitch() const { return m_pitch; }

void Camera::UpdateViewMatrix()
{
    if (m_view_should_update)
    {
        XMVECTOR look = XMLoadFloat3(&m_look);
        XMVECTOR up = XMLoadFloat3(&m_up);
        XMVECTOR right = XMLoadFloat3(&m_right);
        XMVECTOR position = XMLoadFloat3(&m_position);

        // Re-orthorganize the camera basis
        look = XMVector3Normalize(look);
        up = XMVector3Normalize(XMVector3Cross(look, right));
        right = XMVector3Normalize(XMVector3Cross(up, look));

        XMStoreFloat4x4(&m_view, XMMatrixLookToLH(position, look, up));

        XMStoreFloat3(&m_look, look);
        XMStoreFloat3(&m_up, up);
        XMStoreFloat3(&m_right, right);
        XMStoreFloat3(&m_position, position);

        m_view_should_update = false;
    }
}

void Camera::OnMouseMove(float yaw_angle_radians, float pitch_angle_radians)
{
    Pitch(pitch_angle_radians);
    Yaw(yaw_angle_radians);
}
