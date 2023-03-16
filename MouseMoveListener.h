#pragma once
class MouseMoveListener
{
public:
    virtual ~MouseMoveListener() = default;
    virtual void OnMouseMove(float pitch_angle_radians, int rotate_world_axis_angle) = 0;
};
