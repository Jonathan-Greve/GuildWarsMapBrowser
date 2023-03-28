#include "pch.h"
#include "InputManager.h"

InputManager::InputManager()
    : m_mouse_pos{0, 0}
{
}

InputManager::~InputManager() { }

void InputManager::AddMouseMoveListener(MouseMoveListener* listener)
{
    m_mouseMoveListeners.push_back(listener);
}

void InputManager::RemoveMouseMoveListener(MouseMoveListener* listener)
{
    m_mouseMoveListeners.erase(
      std::remove(m_mouseMoveListeners.begin(), m_mouseMoveListeners.end(), listener),
      m_mouseMoveListeners.end());
}

void InputManager::OnKeyDown(WPARAM wParam, HWND hWnd) { }

void InputManager::OnKeyUp(WPARAM wParam, HWND hWnd) { }

void InputManager::OnMouseMove(int x, int y, WPARAM wParam, HWND hWnd)
{
    if ((wParam & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - m_mouse_pos.x));
        float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - m_mouse_pos.y));

        // Call listeners with dx, dy data
        for (MouseMoveListener* listener : m_mouseMoveListeners)
        {
            listener->OnMouseMove(dx, dy);
        }
    }

    m_mouse_pos.x = x;
    m_mouse_pos.y = y;
}

void InputManager::OnMouseDown(int x, int y, WPARAM wParam, HWND hWnd, UINT message)
{
    m_mouse_pos.x = x;
    m_mouse_pos.y = y;

    if (message & WM_RBUTTONDOWN)
    {
        // Hide the cursor
        ShowCursor(FALSE);

        // Get the current cursor position
        GetCursorPos(&right_mouse_button_down_pos);
    }

    SetCapture(hWnd);
}

void InputManager::OnMouseUp(int x, int y, WPARAM wParam, HWND hWnd, UINT message)
{

    if (message & WM_RBUTTONUP)
    {

        // Set the cursor position to the stored position
        SetCursorPos(right_mouse_button_down_pos.x, right_mouse_button_down_pos.y);
    }

    // Show the cursor
    ShowCursor(TRUE);

    // Release the mouse input
    ReleaseCapture();
}

void InputManager::OnMouseWheel(short wheel_delta, HWND hWnd)
{
    // Determine the direction of the mouse wheel rotation
    int scrollDirection = 0;
    if (wheel_delta > 0)
    {
        // The mouse wheel was rotated forward (toward the user)
        scrollDirection = 1;
    }
    else if (wheel_delta < 0)
    {
        // The mouse wheel was rotated backward (away from the user)
        scrollDirection = -1;
    }
}

bool InputManager::IsKeyDown(UINT key) const { return (GetAsyncKeyState(key) & 0x8000); }
