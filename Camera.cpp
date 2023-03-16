#include "pch.h"
#include "Camera.h"

using namespace DirectX;

Camera::Camera() { }

Camera::~Camera() { }

void Camera::Initialize(const XMFLOAT3& position, const XMFLOAT3& target, const XMFLOAT3& up, float fov,
                        float aspectRatio, float nearZ, float farZ)
{
    m_position = position;
    m_target = target;
    m_up = up;
    m_fov = fov;
    m_aspectRatio = aspectRatio;
    m_nearZ = nearZ;
    m_farZ = farZ;
    m_rotation = XMFLOAT3(0, 0, 0);

    XMStoreFloat4x4(
      &m_viewMatrix,
      XMMatrixLookAtLH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_target), XMLoadFloat3(&m_up)));
    XMStoreFloat4x4(&m_projectionMatrix, XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, m_nearZ, m_farZ));
}

void Camera::OnViewPortChanged(float viewport_width, float viewport_height)
{
    // Update the aspect ratio
    m_aspectRatio = viewport_width / viewport_height;

    // Recalculate the projection matrix
    XMStoreFloat4x4(&m_projectionMatrix, XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, m_nearZ, m_farZ));
}

void Camera::LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target)
{
    // Store the position and target in the class members
    m_position = pos;
    m_target = target;

    // Calculate the forward vector from position to target
    XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&target) - XMLoadFloat3(&pos));

    // Calculate the right vector by crossing the forward vector with the world up vector
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&m_world_up), forward));

    // Calculate the up vector by crossing the forward and right vectors
    XMVECTOR up = XMVector3Cross(forward, right);

    // Store the up vector in the class member
    XMStoreFloat3(&m_up, up);

    // Update the view matrix
    XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookAtLH(XMLoadFloat3(&pos), XMLoadFloat3(&target), up));
}

void Camera::SetPosition(const DirectX::XMFLOAT3& position)
{
    // Update the position
    m_position = position;

    // Recalculate the target and up vectors based on the new position
    LookAt(m_position, m_target);
}

void Camera::Update(float deltaTime, bool is_a_key_down, bool is_w_key_down, bool is_s_key_down,
                    bool is_d_key_down)
{
    // Calculate the camera's forward vector
    XMFLOAT3 forward;
    XMStoreFloat3(&forward, XMVector3Normalize(XMLoadFloat3(&m_target) - XMLoadFloat3(&m_position)));

    // Compute the camera's right vector (assuming Y is the up direction)
    XMFLOAT3 right;
    XMStoreFloat3(&right, XMVector3Cross(XMLoadFloat3(&m_world_up), XMLoadFloat3(&forward)));
    XMVECTOR rightNormalized = XMVector3Normalize(XMLoadFloat3(&right));

    // Calculate the camera's up vector by crossing the forward and right vectors
    XMFLOAT3 up;
    XMStoreFloat3(&up, XMVector3Cross(XMLoadFloat3(&forward), rightNormalized));
    m_up = up;

    // Calculate the camera's movement vector
    XMFLOAT3 movement = XMFLOAT3(0, 0, 0);
    if (is_w_key_down)
    {
        movement.x += forward.x * m_walk_speed * deltaTime;
        movement.y += forward.y * m_walk_speed * deltaTime;
        movement.z += forward.z * m_walk_speed * deltaTime;
    }
    if (is_s_key_down)
    {
        movement.x -= forward.x * m_walk_speed * deltaTime;
        movement.y -= forward.y * m_walk_speed * deltaTime;
        movement.z -= forward.z * m_walk_speed * deltaTime;
    }
    if (is_a_key_down)
    {
        movement.x -= right.x * m_strafe_speed * deltaTime;
        movement.y -= right.y * m_strafe_speed * deltaTime;
        movement.z -= right.z * m_strafe_speed * deltaTime;
    }
    if (is_d_key_down)
    {
        movement.x += right.x * m_strafe_speed * deltaTime;
        movement.y += right.y * m_strafe_speed * deltaTime;
        movement.z += right.z * m_strafe_speed * deltaTime;
    }

    // Update position using movement
    m_position.x += movement.x;
    m_position.y += movement.y;
    m_position.z += movement.z;

    // Update view and projection matrices
    XMStoreFloat4x4(
      &m_viewMatrix,
      XMMatrixLookAtLH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_target), XMLoadFloat3(&m_up)));
    XMStoreFloat4x4(&m_projectionMatrix, XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, m_nearZ, m_farZ));
}

void Camera::OnMouseMove(float pitch_angle_radians, int rotate_world_axis_angle)
{
    // Calculate the new pitch and yaw based on the input pitch and yaw changes
    m_rotation.x += pitch_angle_radians;
    m_rotation.y += rotate_world_axis_angle;

    // Clamp the pitch to prevent camera flipping
    m_rotation.x = std::max(std::min(m_rotation.x, XM_PI / 2.0f - 0.01f), -XM_PI / 2.0f + 0.01f);

    // Calculate the new forward vector based on the new pitch and yaw
    XMFLOAT3 forward;
    forward.x = cosf(m_rotation.x) * cosf(m_rotation.y);
    forward.y = sinf(m_rotation.x);
    forward.z = cosf(m_rotation.x) * sinf(m_rotation.y);
    XMVECTOR forward_normalized = XMVector3Normalize(XMLoadFloat3(&forward));

    // Calculate the new target by adding the forward vector to the camera's position
    XMStoreFloat3(&m_target, XMLoadFloat3(&m_position) + forward_normalized);

    // Update the view matrix using the new target
    XMStoreFloat4x4(
      &m_viewMatrix,
      XMMatrixLookAtLH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_target), XMLoadFloat3(&m_up)));
}

DirectX::XMMATRIX Camera::GetViewMatrix() const { return XMLoadFloat4x4(&m_viewMatrix); }

DirectX::XMMATRIX Camera::GetProjectionMatrix() const { return XMLoadFloat4x4(&m_projectionMatrix); }
