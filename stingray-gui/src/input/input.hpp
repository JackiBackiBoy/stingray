#pragma once

#pragma once

#include "keycode.hpp"
#include "mousebutton.hpp"

namespace sr::input {
	struct KeyboardState {
		bool buttons[256] = { false };
	};

	struct MouseState {
		float deltaX = 0.0f;
		float deltaY = 0.0f;
		bool mouse1 = false;
		bool mouse2 = false;
		bool mouse3 = false;
		bool mouse4 = false;
		bool mouse5 = false;
	};

	void initialize(); // NOTE: Required to be called on startup
	// TODO: Combine parseKeyEvent and parseMouseEvent into one function
	void parseKeyDownEvent(void* wParam, void* lParam); // NOTE: Called internally by Stingray
	void parseKeyUpEvent(void* wParam, void* lParam); // NOTE: Called internally by Stingray
	void parseMouseEvent(void* wParam, void* lParam); // NOTE: Called internally by Stingray
	void getKeyboardState(KeyboardState& keyboard);
	void getMouseState(MouseState& mouse);
	void update(); // NOTE: Should ideally be called once every frame

	bool isDown(KeyCode keyCode);
	bool isDown(MouseButton button);
}