#pragma once

#include <string>
#include <windows.h>

#include "../core/window_base.hpp"

namespace sr {
	class WindowWin32 final : public IWindow {
	public:
		WindowWin32(const WindowInfo& info);
		virtual ~WindowWin32();

		bool pollEvents() override;
		bool shouldClose() override;

		int getClientWidth() const override { return m_ClientRect.right - m_ClientRect.left; }
		int getClientHeight() const override { return m_ClientRect.bottom - m_ClientRect.top; }
		int getWindowWidth() const override { return m_WindowRect.right - m_WindowRect.left; }
		int getWindowHeight() const override { return m_WindowRect.bottom - m_WindowRect.top; }
		void* getHandle() override;

	private:
		static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

		WindowInfo m_WindowInfo = {};
		HWND m_Handle = nullptr;
		RECT m_ClientRect = {};
		RECT m_WindowRect = {};
		bool m_ShouldClose = false;

		int m_SizingBorder = 8;
		int m_TitlebarHeight = 30;
	};

	typedef WindowWin32 Window;
}