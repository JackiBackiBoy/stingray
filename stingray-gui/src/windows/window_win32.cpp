#include "window_win32.hpp"

#include "../core/utilities.hpp"
#include "../input/rawinput.hpp"

namespace sr {

	WindowWin32::WindowWin32(const WindowInfo& info) :
		IWindow() {

		std::wstring wTitle = sr::utilities::toWideString(info.title);

		const WNDCLASSEX windowClass = {
					.cbSize = sizeof(WNDCLASSEX),
					.style = CS_OWNDC,
					.lpfnWndProc = WindowProcedure,
					.cbClsExtra = 0,
					.cbWndExtra = 0,
					.hInstance = GetModuleHandle(nullptr),
					.hIcon = nullptr,
					.hCursor = LoadCursor(nullptr, IDC_ARROW),
					.hbrBackground = nullptr,
					.lpszMenuName = nullptr,
					.lpszClassName = wTitle.c_str(),
					.hIconSm = nullptr
		};
		RegisterClassEx(&windowClass);

		const int width = info.width == ~0 ? DEFAULT_WIDTH : info.width;
		const int height = info.height == ~0 ? DEFAULT_HEIGHT : info.height;
		RECT windowRect = { 0, 0, width, height };

		// Use the specified window dimensions as the dimensions for the client area
		if (has_flag(info.flags, WindowFlag::SIZE_IS_CLIENT_AREA) && info.width != ~0 && info.height != ~0) {
			if (has_flag(info.flags, WindowFlag::NO_TITLEBAR)) {
				int borderX = GetSystemMetrics(SM_CXBORDER);
				int borderY = GetSystemMetrics(SM_CYBORDER);
				windowRect.right += borderX * 2;
				windowRect.bottom += borderY * 2;
			}
			else {
				AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
			}
		}

		const int windowWidth = windowRect.right - windowRect.left;
		const int windowHeight = windowRect.bottom - windowRect.top;

		// Window centering
		POINT windowPosition = {
			info.positionX == ~0 ? 0 : info.positionY,
			info.positionY == ~0 ? 0 : info.positionY
		};

		if (has_flag(info.flags, WindowFlag::HCENTER) || has_flag(info.flags, WindowFlag::VCENTER)) {
			const HMONITOR primaryMonitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
			MONITORINFO monitorInfo = { .cbSize = sizeof(monitorInfo) };

			GetMonitorInfo(primaryMonitor, &monitorInfo);
			const int monitorWidth = std::abs(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);
			const int monitorHeight = std::abs(monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);

			if (has_flag(info.flags, WindowFlag::HCENTER) && info.width != ~0) {
				windowPosition.x = monitorInfo.rcMonitor.left + monitorWidth / 2 - windowWidth / 2;
			}

			if (has_flag(info.flags, WindowFlag::VCENTER) && info.height != ~0) {
				windowPosition.y = monitorInfo.rcMonitor.top + monitorHeight / 2 - windowHeight / 2;
			}
		}

		m_Handle = CreateWindowEx(
			0,
			windowClass.lpszClassName,
			wTitle.c_str(),
			WS_OVERLAPPEDWINDOW,
			windowPosition.x,
			windowPosition.y,
			windowWidth,
			windowHeight,
			nullptr,
			nullptr,
			windowClass.hInstance,
			this
		);

		ShowWindow(m_Handle, SW_SHOW);
	}

	WindowWin32::~WindowWin32() {

	}

	bool WindowWin32::pollEvents() {
		bool result = false;
		MSG msg = {};

		if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			result = true;
		}

		if (msg.message == WM_QUIT) {
			m_ShouldClose = true;
		}

		return result;
	}

	bool WindowWin32::shouldClose() {
		return m_ShouldClose;
	}

	void* WindowWin32::getHandle() {
		return (void*)m_Handle;
	}

	LRESULT WindowWin32::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
		case WM_ERASEBKGND:
			return TRUE;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_INPUT:
			sr::rawinput::parseMessage((void*)lParam);
			return 0;
		}

		return DefWindowProc(window, message, wParam, lParam);
	}

}