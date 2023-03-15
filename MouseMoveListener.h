#pragma once
class MouseMoveListener
{
public:
    virtual ~MouseMoveListener() = default;
    virtual void OnMouseMove(float dx, float dy) = 0;
};
