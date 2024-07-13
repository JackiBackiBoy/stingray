#pragma once

#include "keycode.hpp"

namespace sr::rawinput {
	struct RawKeyboardState {
		bool buttons[256] = { false };
		bool downOnceButtons[256] = { false }; // NOTE: Stores down-once events from `isDownOnce`, only used for lookup
	};

	struct RawMouseState {
		//glm::vec2 delta = { 0.0f, 0.0f };
		float deltaX = 0.0f;
		float deltaY = 0.0f;
		float wheelDelta = 0.0f;
		bool mouse1 = false; // Left mouse button
		bool mouse2 = false; // Right mouse button
		bool mouse3 = false; // Middle mouse button
		bool mouse4 = false;
		bool mouse5 = false;
	};

	void initialize();
	void parseMessage(const void* lParam);
	void getKeyboardState(RawKeyboardState& state);
	void getMouseState(RawMouseState& state);
	void update(); // NOTE: Should ideally be called once per frame

	bool isDown(KeyCode keyCode);

	// NOTE: Returns true ONCE if pressed down, and will continue to return false while the key is down.
	// Resets once the key is released. This is useful for toggle behavior
	bool isDownOnce(KeyCode keyCode);
}