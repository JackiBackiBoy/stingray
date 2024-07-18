#include "window_win32.hpp"

#include <windowsx.h>

#include "../core/utilities.hpp"
#include "../input/rawinput.hpp"

namespace sr {

	WindowWin32::WindowWin32(const WindowInfo& info) :
		IWindow(), m_WindowInfo(info) {

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

	static constexpr int hitTestNCA(uint8_t hitstate) {
		if (hitstate >= 0b10000000) { // minimize
			return HTMINBUTTON;
		}

		if (hitstate >= 0b01000000) {
			return HTMAXBUTTON;
		}

		if (hitstate >= 0b00100000) {
			return HTCLOSE;
		}

		if (hitstate >= 0b010000 && hitstate < 0b100000) {
			return HTCAPTION;
		}

		switch (hitstate) {
		case 0b00000:
			return HTNOWHERE;
		case 0b01000:
			return HTTOP;
		case 0b00100:
			return HTLEFT;
		case 0b00010:
			return HTBOTTOM;
		case 0b00001:
			return HTRIGHT;
		case 0b01100:
			return HTTOPLEFT;
		case 0b01001:
			return HTTOPRIGHT;
		case 0b00110:
			return HTBOTTOMLEFT;
		case 0b00011:
			return HTBOTTOMRIGHT;
		default:
			return HTNOWHERE;
		}
	}

	LRESULT WindowWin32::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
		WindowWin32* pWindow = reinterpret_cast<WindowWin32*>(GetWindowLongPtr(window, GWLP_USERDATA));

		switch (message) {
		case WM_NCCREATE:
			{
			CREATESTRUCT* pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
			pWindow = reinterpret_cast<WindowWin32*>(pCreateStruct->lpCreateParams);
			SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)pWindow);
			}
			break;
		case WM_ERASEBKGND:
			return TRUE;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_INPUT:
			sr::rawinput::parseMessage((void*)lParam);
			return 0;
		case WM_NCCALCSIZE:
			{
				if (!has_flag(pWindow->m_WindowInfo.flags, WindowFlag::NO_TITLEBAR)) {
					break;
				}

				const UINT dpi = GetDpiForWindow(window);
				const int frameX = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
				const int frameY = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
				const int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

				NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lParam;
				RECT* requestedClientRect = params->rgrc;

				bool isMaximized = false;
				WINDOWPLACEMENT placement = { 0 };
				placement.length = sizeof(WINDOWPLACEMENT);
				if (GetWindowPlacement(window, &placement)) {
					isMaximized = placement.showCmd == SW_SHOWMAXIMIZED;
				}

				if (isMaximized) {
					int sizeFrameY = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi);
					requestedClientRect->right -= frameY + padding;
					requestedClientRect->left += frameX + padding;
					requestedClientRect->top += sizeFrameY + padding;
					requestedClientRect->bottom -= frameY + padding;
				}
				else {
					// ------ Hack to remove the title bar (non-client) area ------
					// In order to hide the standard title bar we must change
					// the NCCALCSIZE_PARAMS, which dictates the title bar area.
					//
					// In Windows 10 we can set the top component to '0', which
					// in effect hides the default title bar.
					// However, for unknown reasons this does not work in
					// Windows 11, instead we are required to set the top
					// component to '1' in order to get the same effect.
					//
					// Thus, we must first check the Windows version before
					// altering the NCCALCSIZE_PARAMS structure.
					const int cxBorder = 1;
					const int cyBorder = 1;
					InflateRect((LPRECT)lParam, -cxBorder, -cyBorder);
				}

				return 0;
			}
			break;
		case WM_NCHITTEST:
			{
				if (!has_flag(pWindow->m_WindowInfo.flags, WindowFlag::NO_TITLEBAR)) {
					break;
				}

				const POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				RECT rcWindow{};
				GetWindowRect(window, &rcWindow);

				const int sizingBorder = pWindow->m_SizingBorder;
				const int titlebarHeight = pWindow->m_TitlebarHeight;

				const uint8_t top = ptMouse.y >= rcWindow.top && ptMouse.y < rcWindow.top + sizingBorder;
				const uint8_t left = ptMouse.x >= rcWindow.left && ptMouse.x < rcWindow.left + sizingBorder;
				const uint8_t bottom = ptMouse.y <= rcWindow.bottom && ptMouse.y > rcWindow.bottom - sizingBorder;
				const uint8_t right = ptMouse.x <= rcWindow.right && ptMouse.x > rcWindow.right - sizingBorder;
				const uint8_t caption = !top && !left && !right && ptMouse.y <= rcWindow.top + titlebarHeight;
				//const uint8_t minimize = !top && !left && !right && ptMouse.x >= rcWindow.right - WINDOW_SYSBUTTON_WIDTH * 3 &&
				//	ptMouse.x < rcWindow.right - WINDOW_SYSBUTTON_WIDTH * 2 && ptMouse.y <= rcWindow.top + WINDOW_TITLEBAR_HEIGHT;
				//const uint8_t maximize = !top && !left && !right && ptMouse.x >= rcWindow.right - WINDOW_SYSBUTTON_WIDTH * 2 &&
				//	ptMouse.x < rcWindow.right - WINDOW_SYSBUTTON_WIDTH && ptMouse.y <= rcWindow.top + WINDOW_TITLEBAR_HEIGHT;
				//const uint8_t close = !top && !left && !right && ptMouse.x >= rcWindow.right - WINDOW_SYSBUTTON_WIDTH &&
				//	ptMouse.x < rcWindow.right && ptMouse.y <= rcWindow.top + WINDOW_TITLEBAR_HEIGHT;

				uint8_t hitstate = /*(minimize << 7) | (maximize << 6) | (close << 5) |*/ (caption << 4) | (top << 3) | (left << 2) | (bottom << 1) | (right);

				return hitTestNCA(hitstate);
			}
			break;
		}

		return DefWindowProc(window, message, wParam, lParam);
	}

}