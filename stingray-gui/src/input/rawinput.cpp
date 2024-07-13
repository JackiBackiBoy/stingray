#include "rawinput.hpp"

#include <Windows.h>
#include <vector>
#include <cassert>

namespace sr::rawinput {
	static RawMouseState g_Mouse{};
	static RawKeyboardState g_Keyboard{};
	static std::vector<RAWINPUT*> g_Messages{};

	void initialize() {
		// Raw input device registration
		RAWINPUTDEVICE rid[2] = {};

		// Register mouse
		rid[0].usUsagePage = 0x01;
		rid[0].usUsage = 0x02; // HID_USAGE_GENERIC_MOUSE
		rid[0].dwFlags = 0;
		rid[0].hwndTarget = NULL;

		// Register keyboard
		rid[1].usUsagePage = 0x01;
		rid[1].usUsage = 0x06; // HID_USAGE_GENERIC_KEYBOARD
		rid[1].dwFlags = 0;
		rid[1].hwndTarget = NULL;

		RegisterRawInputDevices(rid, 2, sizeof(rid[0]));
	}

	void parseRawInput(RAWINPUT* raw) {
		if (raw->header.dwType == RIM_TYPEKEYBOARD) {
			const RAWKEYBOARD& rawkeyboard = raw->data.keyboard;

			if (rawkeyboard.VKey >= 0xFF) { // ignore keys not mapped to any VK code
				return;
			}

			uint16_t vkCode = rawkeyboard.VKey;
			uint16_t scanCode = rawkeyboard.MakeCode;

			scanCode |= (rawkeyboard.Flags & RI_KEY_E0) ? 0xE000 : 0;
			scanCode |= (rawkeyboard.Flags & RI_KEY_E1) ? 0xE100 : 0;

			// Find which Alt, Control or Shift key was pressed (left or right)
			switch (vkCode) {
			case VK_SHIFT:
			case VK_CONTROL:
			case VK_MENU:
				vkCode = MapVirtualKey(scanCode, MAPVK_VSC_TO_VK_EX);
				break;
			}

			if (rawkeyboard.Flags == RI_KEY_MAKE) { // key down
				g_Keyboard.buttons[vkCode] = true;
			}
			else if (rawkeyboard.Flags == RI_KEY_BREAK) { // key up
				g_Keyboard.buttons[vkCode] = false;
			}
		}
		else if (raw->header.dwType == RIM_TYPEMOUSE) {
			g_Mouse.deltaX = raw->data.mouse.lLastX;
			g_Mouse.deltaY = raw->data.mouse.lLastY;

			if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
				g_Mouse.wheelDelta += float((SHORT)raw->data.mouse.usButtonData) / float(WHEEL_DELTA);
			}

			// Mouse 1
			if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
				g_Mouse.mouse1 = true;
			}
			else if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
				g_Mouse.mouse1 = false;
			}


			// Mouse 2
			if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
				g_Mouse.mouse2 = true;
			}
			else if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
				g_Mouse.mouse2 = false;
			}

			// Mouse 3
			if (raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
				g_Mouse.mouse3 = true;
			}
			else if (raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) {
				g_Mouse.mouse3 = false;
			}

			// Mouse 4
			if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) {
				g_Mouse.mouse4 = true;
			}
			else if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP) {
				g_Mouse.mouse4 = false;
			}

			// Mouse 5
			if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) {
				g_Mouse.mouse5 = true;
			}
			else if (raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP) {
				g_Mouse.mouse5 = false;
			}
		}
	}

	void parseMessage(const void* lParam) {
		UINT size = 0;
		UINT result = 0;

		result = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
		assert(result == 0);

		RAWINPUT* input = (RAWINPUT*)malloc(size);
		result = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, input, &size, sizeof(RAWINPUTHEADER));

		if (result == size) {
			g_Messages.push_back(input);
			return;
		}

		free(input);
	}

	void getKeyboardState(RawKeyboardState& state) {
		state = g_Keyboard;
	}

	void getMouseState(RawMouseState& state) {
		state = g_Mouse;
	}

	void update() {
		g_Mouse.deltaX = 0.0f;
		g_Mouse.deltaY = 0.0f;
		g_Mouse.wheelDelta = 0.0f;

		getKeyboardState(g_Keyboard);
		getMouseState(g_Mouse);

		for (auto input : g_Messages) {
			parseRawInput(input);
			free(input);
		}

		g_Messages.clear();
	}

	bool isDown(KeyCode keyCode) {
		return g_Keyboard.buttons[(uint16_t)keyCode];
	}

	bool isDownOnce(KeyCode keyCode) {
		if (isDown(keyCode) && !g_Keyboard.downOnceButtons[(uint16_t)keyCode]) {
			g_Keyboard.downOnceButtons[(uint16_t)keyCode] = true;
			return true;
		}

		if (!isDown(keyCode) && g_Keyboard.downOnceButtons[(uint16_t)keyCode]) {
			g_Keyboard.downOnceButtons[(uint16_t)keyCode] = false;
		}

		return false;
	}

}