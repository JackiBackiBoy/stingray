#pragma once

#include "../core/window_base.hpp"

namespace sr {
	class WindowWin32 final : public IWindow {
	public:
		WindowWin32(const WindowInfo& info);
		virtual ~WindowWin32();

		void pollEvents() override;
		void show() override;
		bool shouldClose() override;

		int getClientWidth() const override;
		int getClientHeight() const override;
		int getWindowWidth() const override;
		int getWindowHeight() const override;
		void* getHandle() override;

	private:
		struct Impl;
		Impl* m_Impl;
	};

	typedef WindowWin32 Window;
}