#include "ui_event.hpp"

namespace sr {

	UIEvent::UIEvent(UIEventType type) {
		setType(type);
	}

	void UIEvent::setType(UIEventType type) {
		const uint32_t numType = static_cast<uint32_t>(type);

		if (type == m_Type) {
			return;
		}

		m_Type = type;

		if ((numType & m_EventMask) != 0) {
			return;
		}

		if ((numType & MOUSE_EVENT_MASK) != 0) { // mouse-related type
			m_EventMask = MOUSE_EVENT_MASK;
			m_Data = std::make_shared<MouseEventData>();
		}
		//else if ((numType & KEYBOARD_EVENT_MASK) != 0) { // keyboard-related type
		//	m_EventMask = KEYBOARD_EVENT_MASK;
		//	m_Data = std::make_shared<KeyboardEventData>();
		//}
		//else if ((numType & NON_PURE_KEYBOARD_EVENT_MASK) != 0) { // non-pure keyboard-related type
		//	m_EventMask = NON_PURE_KEYBOARD_EVENT_MASK;
		//	m_Data = std::make_shared<KeyboardCharData>();
		//}
	}

}