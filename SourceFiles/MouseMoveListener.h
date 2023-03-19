#pragma once
class MouseMoveListener
{
public:
    virtual ~MouseMoveListener() = default;
    virtual void OnMouseMove(float yaw_angle_radians, float pitch_angle_radians) = 0;
};
