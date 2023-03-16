#pragma once
#include "MouseMoveListener.h"

class Camera : public MouseMoveListener
{
public:
    Camera();
    ~Camera();

    void Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target,
                    const DirectX::XMFLOAT3& up, float fov, float aspectRatio, float nearZ, float farZ);
    void OnViewPortChanged(float viewport_width, float viewport_height);
    void Update(float deltaTime, bool is_a_key_down, bool is_w_key_down, bool is_s_key_down,
                bool is_d_key_down);

    virtual void OnMouseMove(float dx, float dy) override;

    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;

private:
    float m_walk_speed = 100;
    float m_strafe_speed = 100;

    DirectX::XMFLOAT3 m_world_up{0.0f, 1.0f, 0.0f};
    DirectX::XMFLOAT3 m_rotation;
    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_target;
    DirectX::XMFLOAT3 m_up;

    float m_fov;
    float m_aspectRatio;
    float m_nearZ;
    float m_farZ;

    DirectX::XMFLOAT4X4 m_viewMatrix;
    DirectX::XMFLOAT4X4 m_projectionMatrix;
};
