#pragma once

#include <cstdint>

namespace sr {
	enum class KeyCode : uint16_t {
		None = 0x00,

		MouseButtonLeft = 0x01,
		MouseButtonRight = 0x02,
		// VK_CANCEL ignored (0x03)
		MouseButtonMiddle = 0x04,
		MouseButton4 = 0x05,
		MouseButton5 = 0x06,
		// 0x07 reserved
		Backspace = 0x08,
		Tab,
		// 0x0A reserved
		// 0x0B reserved
		// VK_CLEAR ignored (0x0C)
		Enter = 0x0D,
		// 0x0E unassigned
		// 0x0F unassigned
		// 0x10 ignored (VK_SHIFT)
		// 0x11 ignored (VK_CONTROl)
		// 0x12 ignored (VK_MENU)
		Pause = 0x13,
		CapsLock = 0x14,
		// 0x15 ignored (VK_KANA / VK_HANGUL)
		// 0x16 ignored (VK_IME_ON)
		// 0x17 ignored (VK_JUNJA)
		// 0x18 ignored (VK_FINAL)
		// 0x19 ignored (VK_HANJA / VK_KANJI)
		// 0x1A ignored (VK_IME_OFF)
		Escape = 0x1B,
		// 0x1C ignored (VK_CONVERT)
		// 0x1D ignored (VK_NONCONVERT)
		// 0x1E ignored (VK_ACCEPT)
		// 0x1F ignored (VK_MODECHANGE)
		Space = 0x20,
		PageUp,
		PageDown,
		End,
		Home,
		Left,
		Up,
		Right,
		Down,
		// 0x29 ignored (VK_SELECT)
		// 0x2A ignored (VK_PRINT)
		// 0x2B ignored (VK_EXECUTE)
		PrintScreen = 0x2C,
		Insert,
		Delete,
		// 0x2F ignored (VK_HELP)
		Alpha0 = 0x30,
		Alpha1,
		Alpha2,
		Alpha3,
		Alpha4,
		Alpha5,
		Alpha6,
		Alpha7,
		Alpha8,
		Alpha9,
		// 0x3A - 0x40 undefined
		A = 0x41,
		B,
		C,
		D,
		E,
		F,
		G,
		H,
		I,
		J,
		K,
		L,
		M,
		N,
		O,
		P,
		Q,
		R,
		S,
		T,
		U,
		V,
		W,
		X,
		Y,
		Z,
		// 0x5B ignored (VK_LWIN)
		// 0x5C ignored (VK_RWIN)
		// 0x5D ignored (VK_APPS)
		// 0x5E reserved
		// 0x5F ignored (VK_SLEEP)
		Numpad0 = 0x60,
		Numpad1,
		Numpad2,
		Numpad3,
		Numpad4,
		Numpad5,
		Numpad6,
		Numpad7,
		Numpad8,
		Numpad9,
		Multiply,
		Add,
		Separator,
		Subtract,
		Decimal,
		Divide,
		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		F13,
		F14,
		F15,
		F16,
		F17,
		F18,
		F19,
		F20,
		F21,
		F22,
		F23,
		F24,
		// 0x88 - 0x8F reserved
		// 0x90 ignored (VK_NUMLOCK)
		// 0x91 ignored (VK_SCROLL)
		// 0x92 - 0x96 ignored (OEM specific)
		// 0x97 - 0x9F unassigned
		LeftShift = 0xA0,
		RightShift,
		LeftControl,
		RightControl,
		LeftAlt,
		RightAlt,
		// 0xA6 - 0xB7 ignored (seriously, who the fuck uses these keys and why are they here?!)
		// 0xB8 - 0xB9 reserved
		// 0xBA - 0xFE ignored (fuck OEM specific keys and antiquated keys)
	};
}