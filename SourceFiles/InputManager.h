#pragma once
#include "MouseMoveListener.h"

class InputManager
{
public:
    InputManager(HWND hWnd);
    ~InputManager();

    void ProcessRawInput(LPARAM lParam);

    void AddMouseMoveListener(MouseMoveListener* listener);
    void RemoveMouseMoveListener(MouseMoveListener* listener);

    void OnKeyDown(WPARAM wParam, HWND hWnd);
    void OnKeyUp(WPARAM wParam, HWND hWnd);
    void OnRawMouseMove(int dx, int dy);

    void OnMouseDown(int x, int y, WPARAM wParam, HWND hWnd, UINT message);

    void OnMouseUp(int x, int y, WPARAM wParam, HWND hWnd, UINT message);

    void OnMouseWheel(short wheel_delta, HWND hWnd);

    bool IsKeyDown(UINT key) const;

    void TrackMouseLeave(HWND hWnd);
	void OnMouseLeave(HWND hWnd);

    POINT GetClientCoords(HWND hWnd);

	POINT GetWindowCenter(HWND hWnd);

private:
    RAWINPUTDEVICE rid;

    POINT m_mouse_pos;
    POINT m_right_mouse_button_down_pos;
    std::vector<MouseMoveListener*> m_mouseMoveListeners;
    bool m_isSettingCursorPosition = false;


    bool m_isCursorShown = true;
};
