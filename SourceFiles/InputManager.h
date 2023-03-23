#pragma once
#include "MouseMoveListener.h"

class InputManager
{
public:
    InputManager();
    ~InputManager();

    void AddMouseMoveListener(MouseMoveListener* listener);
    void RemoveMouseMoveListener(MouseMoveListener* listener);

    void OnKeyDown(WPARAM wParam, HWND hWnd);
    void OnKeyUp(WPARAM wParam, HWND hWnd);
    void OnMouseMove(int x, int y, WPARAM wParam, HWND hWnd);

    void OnMouseDown(int x, int y, WPARAM wParam, HWND hWnd, UINT message);

    void OnMouseUp(int x, int y, WPARAM wParam, HWND hWnd, UINT message);

    void OnMouseWheel(short wheel_delta, HWND hWnd);

    bool IsKeyDown(UINT key) const;

private:
    POINT m_mouse_pos;
    POINT right_mouse_button_down_pos;
    std::vector<MouseMoveListener*> m_mouseMoveListeners;
};
