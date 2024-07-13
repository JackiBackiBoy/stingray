#pragma once

#include <string>
#include <windows.h>

#include "../core/window_base.hpp"

namespace sr {
	class WindowWin32 final : public IWindow {
	public:
		WindowWin32(const std::string& title, int width, int height);
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

		HWND m_Handle = nullptr;
		RECT m_ClientRect = {};
		RECT m_WindowRect = {};
		bool m_ShouldClose = false;
	};

	typedef WindowWin32 Window;
}