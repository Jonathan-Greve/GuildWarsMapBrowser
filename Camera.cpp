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

void Camera::OnMouseMove(float dx, float dy)
{
    // Update the camera's rotation based on the mouse movement
    m_rotation.x += dy;
    m_rotation.y += dx;

    // Clamp the pitch rotation to prevent camera flipping
    m_rotation.x = std::max(std::min(m_rotation.x, XM_PI / 2.0f - 0.01f), -XM_PI / 2.0f + 0.01f);

    // Calculate the new target position based on the camera's updated rotation
    XMFLOAT3 newTarget;
    newTarget.x = m_position.x + sinf(m_rotation.y) * cosf(m_rotation.x);
    newTarget.y = m_position.y + sinf(m_rotation.x);
    newTarget.z = m_position.z + cosf(m_rotation.y) * cosf(m_rotation.x);

    m_target = newTarget;
}
DirectX::XMMATRIX Camera::GetViewMatrix() const { return XMLoadFloat4x4(&m_viewMatrix); }

DirectX::XMMATRIX Camera::GetProjectionMatrix() const { return XMLoadFloat4x4(&m_projectionMatrix); }
