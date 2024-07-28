#pragma once

#include <string>
#include <windows.h>

#include "../core/window_base.hpp"
#include "../ui/ui_event.hpp"

namespace sr {
	class WindowWin32 final : public IWindow {
	public:
		WindowWin32(const WindowInfo& info);
		virtual ~WindowWin32();

		void pollEvents() override;
		void show() override;
		bool shouldClose() override;

		int getClientWidth() const override { return m_ClientRect.right - m_ClientRect.left; }
		int getClientHeight() const override { return m_ClientRect.bottom - m_ClientRect.top; }
		int getWindowWidth() const override { return m_WindowRect.right - m_WindowRect.left; }
		int getWindowHeight() const override { return m_WindowRect.bottom - m_WindowRect.top; }
		void* getHandle() override;

	private:
		static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

		UIEvent createMouseEvent(UINT message, WPARAM wParam, LPARAM lParam);

		WindowInfo m_WindowInfo = {};
		HWND m_Handle = nullptr;
		RECT m_ClientRect = {};
		RECT m_WindowRect = {};
		bool m_ShouldClose = false;

		int m_SizingBorder = 8;
		int m_TitlebarHeight = 31;

		UIEvent m_MouseButtonEvent = { UIEventType::None };
		bool m_TrackingMouseLeave = false;
		bool m_EnteringWindow = false;
	};

	typedef WindowWin32 Window;
}