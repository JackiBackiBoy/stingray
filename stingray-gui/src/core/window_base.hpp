#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "enum_flags.hpp"

namespace sr {
	enum class WindowFlag : uint32_t {
		NONE = 0,
		RESIZEABLE = 1 << 0,
		HCENTER = 1 << 1,
		VCENTER = 1 << 2,
		SIZE_IS_CLIENT_AREA = 1 << 3,
		NO_TITLEBAR = 1 << 4
	};

	template<>
	struct enable_bitmask_operators<WindowFlag> { static const bool enable = true; };

	struct WindowInfo {
		std::string title = "";
		int width = ~0;
		int height = ~0;
		int positionX = ~0;
		int positionY = ~0;
		WindowFlag flags = WindowFlag::NONE;
	};

	class IWindow {
	public:
		IWindow() {}
		virtual ~IWindow() {}

		virtual void pollEvents() = 0;
		virtual void show() = 0;
		virtual bool shouldClose() = 0;

		virtual int getClientWidth() const = 0;
		virtual int getClientHeight() const = 0;
		virtual int getWindowWidth() const = 0;
		virtual int getWindowHeight() const = 0;
		virtual void* getHandle() = 0;

	protected:
		static constexpr int DEFAULT_WIDTH = 512;
		static constexpr int DEFAULT_HEIGHT = 512;
	};
}