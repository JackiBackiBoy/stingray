#pragma once

#include <functional>

namespace sr {
	class IWindow {
	public:
		IWindow() {}
		virtual ~IWindow() {}

		virtual bool pollEvents() = 0;
		virtual bool shouldClose() = 0;

		virtual int getClientWidth() const = 0;
		virtual int getClientHeight() const = 0;
		virtual int getWindowWidth() const = 0;
		virtual int getWindowHeight() const = 0;
		virtual void* getHandle() = 0;
	};
}