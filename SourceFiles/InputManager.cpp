#include "pch.h"
#include "InputManager.h"

InputManager::InputManager(HWND hWnd)
	: m_mouse_pos{0, 0}
{
	rid.usUsagePage = 0x01; // Generic Desktop Controls
    rid.usUsage = 0x02;    // Mouse
    rid.dwFlags = 0;
    rid.hwndTarget = hWnd; // Handle to your window
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

InputManager::~InputManager() { }

void InputManager::ProcessRawInput(LPARAM lParam)
{
    UINT dwSize = sizeof(RAWINPUT);
    static BYTE lpb[sizeof(RAWINPUT)];

    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
    RAWINPUT* raw = (RAWINPUT*)lpb;

    if (raw->header.dwType == RIM_TYPEMOUSE) 
    {
        int dx = raw->data.mouse.lLastX;
        int dy = raw->data.mouse.lLastY;

        OnRawMouseMove(dx, dy);
    }
}

void InputManager::AddMouseMoveListener(MouseMoveListener* listener) { m_mouseMoveListeners.push_back(listener); }

void InputManager::RemoveMouseMoveListener(MouseMoveListener* listener) { std::erase(m_mouseMoveListeners, listener); }

void InputManager::OnKeyDown(WPARAM wParam, HWND hWnd) { }

void InputManager::OnKeyUp(WPARAM wParam, HWND hWnd) { }

void InputManager::OnRawMouseMove(int dx, int dy)
{
    if (m_isCursorShown) return; // Ignore if cursor is shown

    float radianX = DirectX::XMConvertToRadians(0.25f * dx);
    float radianY = DirectX::XMConvertToRadians(0.25f * dy);

    for (MouseMoveListener* listener : m_mouseMoveListeners)
    {
        listener->OnMouseMove(radianX, radianY);
    }

	SetCursorPos(m_right_mouse_button_down_pos.x, m_right_mouse_button_down_pos.y);
}

void InputManager::OnMouseDown(int x, int y, WPARAM wParam, HWND hWnd, UINT message)
{
	TrackMouseLeave(hWnd);

	m_mouse_pos.x = x;
	m_mouse_pos.y = y;

	if (message == WM_RBUTTONDOWN)
	{
		if (m_isCursorShown)
		{
			// Hide the cursor
			ShowCursor(FALSE);
			m_isCursorShown = false;
		}

		// Get the current cursor position
		GetCursorPos(&m_right_mouse_button_down_pos);
	}

	SetCapture(hWnd);
}

void InputManager::OnMouseUp(int x, int y, WPARAM wParam, HWND hWnd, UINT message)
{
	if (message == WM_RBUTTONUP)
	{
		// Set the cursor position to the stored position
		SetCursorPos(m_right_mouse_button_down_pos.x, m_right_mouse_button_down_pos.y);

		if (!m_isCursorShown)
		{
			// Show the cursor
			ShowCursor(TRUE);
			m_isCursorShown = true;
		}
	}

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

void InputManager::OnMouseLeave(HWND hWnd)
{
    if (!m_isCursorShown)
    {
        // Show the cursor
        ShowCursor(TRUE);
        m_isCursorShown = true;
    }
}

void InputManager::TrackMouseLeave(HWND hWnd)
{
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hWnd;
    TrackMouseEvent(&tme);
}

bool InputManager::IsKeyDown(UINT key) const { return (GetAsyncKeyState(key) & 0x8000); }

POINT InputManager::GetClientCoords(HWND hWnd)
{
	SetCapture(hWnd);
	POINT screenPoint;
	GetCursorPos(&screenPoint);
	ScreenToClient(hWnd, &screenPoint);


	ReleaseCapture();

	return screenPoint;
}

POINT InputManager::GetWindowCenter(HWND hWnd)
{
    RECT rect;
    GetClientRect(hWnd, &rect);
    POINT center;
    center.x = (rect.right - rect.left) / 2;
    center.y = (rect.bottom - rect.top) / 2;
    ClientToScreen(hWnd, &center);
    return center;
}

