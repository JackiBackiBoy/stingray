#include "../input.hpp"

#include <optional>

#include <Windows.h>
#include <windowsx.h>

namespace sr::input {
	static KeyboardState g_CurrentKeyboard = {}; // NOTE: Only updated when update() is called
	static MouseState g_CurrentMouse = {}; // NOTE: Onlyupdated when update() is called

	static KeyboardState g_WorkingKeyboard = {}; // NOTE: Updated when parseKeyEvent() functions are called
	static MouseState g_WorkingMouse = {}; // NOTE: Updated when parseMouseEvent() is called

	static std::optional<POINT> g_LastMousePos = std::nullopt;
	static std::optional<POINT> g_CurrentMousePos = std::nullopt;

	void initialize() {

	}

	void parseKeyDownEvent(void* wParam, void* lParam) {
		WPARAM trueWParam = (WPARAM)wParam;
		LPARAM trueLParam = (LPARAM)lParam;

		g_WorkingKeyboard.buttons[trueWParam] = true;
	}

	void parseKeyUpEvent(void* wParam, void* lParam) {
		WPARAM trueWParam = (WPARAM)wParam;
		LPARAM trueLParam = (LPARAM)lParam;

		g_WorkingKeyboard.buttons[trueWParam] = false;
	}

	void parseMouseEvent(void* wParam, void* lParam) {
		WPARAM trueWParam = (WPARAM)wParam;
		LPARAM trueLParam = (LPARAM)lParam;

		g_LastMousePos = g_CurrentMousePos;

		const int x = GET_X_LPARAM(trueLParam);
		const int y = GET_Y_LPARAM(trueLParam);

		g_CurrentMousePos = { x, y };

		if (!g_LastMousePos.has_value()) {
			g_LastMousePos = g_CurrentMousePos;
		}

		// Mouse buttons
		g_WorkingMouse.mouse1 = (GET_KEYSTATE_WPARAM(wParam) & MK_LBUTTON) > 0;
		g_WorkingMouse.mouse2 = (GET_KEYSTATE_WPARAM(wParam) & MK_RBUTTON) > 0;
		g_WorkingMouse.mouse3 = (GET_KEYSTATE_WPARAM(wParam) & MK_MBUTTON) > 0;
	}

	void getKeyboardState(KeyboardState& keyboard) {
		keyboard = g_CurrentKeyboard;
	}

	void getMouseState(MouseState& mouse) {
		mouse = g_CurrentMouse;
	}

	void update() {
		g_CurrentKeyboard = g_WorkingKeyboard;
		g_CurrentMouse = g_WorkingMouse;

		if (!g_LastMousePos.has_value() || !g_CurrentMousePos.has_value()) {
			g_CurrentMouse.deltaX = 0;
			g_CurrentMouse.deltaY = 0;
			return;
		}

		g_CurrentMouse.deltaX = g_CurrentMousePos.value().x - g_LastMousePos.value().x;
		g_CurrentMouse.deltaY = g_CurrentMousePos.value().y - g_LastMousePos.value().y;

		g_LastMousePos = g_CurrentMousePos;
	}

	bool isDown(KeyCode keyCode) {
		return g_CurrentKeyboard.buttons[(size_t)keyCode];
	}

	bool isDown(MouseButton button) {
		return true;
	}

}