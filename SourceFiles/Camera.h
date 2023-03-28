#pragma once
#include "MouseMoveListener.h"

using namespace DirectX;

enum class CameraType
{
    Perspective,
    Orthographic
};

enum class WalkDirection
{
    Forward,
    Backward
};

enum class StrafeDirection
{
    Left,
    Right
};

class Camera : public MouseMoveListener
{
public:
    Camera();
    ~Camera();

    void Update(float dt);

    XMVECTOR GetPosition() const;
    XMFLOAT3 GetPosition3f() const;

    void SetPosition(float x, float y, float z);
    XMVECTOR GetRight() const;
    XMFLOAT3 GetRight3f() const;

    XMVECTOR GetUp() const;
    XMFLOAT3 GetUp3f() const;

    XMVECTOR GetLook() const;
    XMFLOAT3 GetLook3f() const;

    void SetFrustumAsPerspective(float fovY, float aspect, float zn, float zf);
    void SetFrustumAsOrthographic(float view_width, float view_height, float zn, float zf);
    void LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp);
    void SetOrientation(float pitch, float yaw);
    void Strafe(StrafeDirection strafe_direction, double dt);
    void Walk(WalkDirection walk_direction, double dt);
    void Pitch(float angle);
    void Yaw(float angle);

    XMMATRIX GetView() const;
    XMFLOAT4X4 GetView4x4() const;
    XMMATRIX GetProj() const;
    XMFLOAT4X4 GetProj4x4() const;

    void OnViewPortChanged(const float viewport_width, const float viewport_height);

    virtual void OnMouseMove(float yaw_angle_radians, float pitch_angle_radians) override;

    float GetAspectRatio() const;
    float GetFovY() const;
    float GetViewWidth() const;
    float GetViewHeight() const;
    float GetNearZ() const;
    float GetFarZ() const;
    CameraType GetCameraType() const;

    float GetYaw() const;

    float GetPitch() const;

    float m_walk_speed = 60.0f;
    float m_strafe_speed = 60.0f;

private:
    void UpdateViewMatrix();

    // Set for perspective projection
    float m_fov = 70 * XM_PI / 180;
    float m_aspectRatio = 1;

    // Set for orthographic projection
    float m_viewWidth = 100000;
    float m_viewHeight = 100000;

    // Set for both projection types
    float m_nearZ = 0.1;
    float m_farZ = 200000;

    XMFLOAT3 m_position;
    XMFLOAT3 m_right;
    XMFLOAT3 m_up;
    XMFLOAT3 m_look;

    float m_yaw;
    float m_pitch;

    XMFLOAT4X4 m_view;
    XMFLOAT4X4 m_proj;

    CameraType m_camera_type = CameraType::Perspective;

    bool m_view_should_update = false;
};
