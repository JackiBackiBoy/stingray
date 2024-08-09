#include "window_win32.hpp"

#include <Windows.h>
#include <windowsx.h>

#include "../core/utilities.hpp"
#include "../input/input.hpp"
#include "../rendering/renderpasses/ui_pass.hpp"
#include "../ui/ui_event.hpp"

namespace sr {
	/* ---------------------------------------------------------------------- */
	/*                       WindowWin32 Implementation                       */
	/* ---------------------------------------------------------------------- */
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

	struct WindowWin32::Impl {
		static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

		void initialize(const WindowInfo& info);
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

	LRESULT WindowWin32::Impl::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
		WindowWin32::Impl* pWindow = reinterpret_cast<WindowWin32::Impl*>(GetWindowLongPtr(window, GWLP_USERDATA));

		switch (message) {
		case WM_NCCREATE:
		{
			CREATESTRUCT* pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
			pWindow = reinterpret_cast<WindowWin32::Impl*>(pCreateStruct->lpCreateParams);
			SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)pWindow);
		}
		break;
		case WM_ERASEBKGND:
			return TRUE;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_XBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_XBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
		case WM_MOUSEMOVE:
		case WM_MOUSELEAVE:
		{
			if (message == WM_MOUSEMOVE) {
				sr::input::parseMouseEvent((void*)wParam, (void*)lParam);
			}

			if (pWindow != nullptr) {
				UIEvent event = pWindow->createMouseEvent(message, wParam, lParam);
				sr::uipass::processEvent(event);
			}
		}
		break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			sr::input::parseKeyDownEvent((void*)wParam, (void*)lParam);
		}
		break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			sr::input::parseKeyUpEvent((void*)wParam, (void*)lParam);
		}
		break;
		case WM_SETFOCUS:
		{
			if (pWindow != nullptr) {
				pWindow->m_EnteringWindow = true;
			}
		}
		break;
		case WM_NCCALCSIZE:
			{
				if (pWindow != nullptr && !has_flag(pWindow->m_WindowInfo.flags, WindowFlag::NO_TITLEBAR)) {
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

				const bool insideWindow = ptMouse.x >= rcWindow.left && ptMouse.x <= rcWindow.right &&
					ptMouse.y >= rcWindow.top && ptMouse.y <= rcWindow.bottom;
				const int sizingBorder = pWindow->m_SizingBorder;
				const int titlebarHeight = pWindow->m_TitlebarHeight;

				const uint8_t top = ptMouse.y >= rcWindow.top && ptMouse.y < rcWindow.top + sizingBorder;
				const uint8_t left = ptMouse.x >= rcWindow.left && ptMouse.x < rcWindow.left + sizingBorder;
				const uint8_t bottom = ptMouse.y <= rcWindow.bottom && ptMouse.y > rcWindow.bottom - sizingBorder;
				const uint8_t right = ptMouse.x <= rcWindow.right && ptMouse.x > rcWindow.right - sizingBorder;
				const uint8_t caption = !top && !left && !right && ptMouse.y <= rcWindow.top + titlebarHeight;

				uint8_t hitstate = (caption << 4) | (top << 3) | (left << 2) | (bottom << 1) | (right);

				int result = hitTestNCA(hitstate);

				if (result == HTNOWHERE && insideWindow) {
					result = HTCLIENT;
				}

				return result;
			}
			break;
		}

		return DefWindowProc(window, message, wParam, lParam);
	}

	void WindowWin32::Impl::initialize(const WindowInfo& info) {
		m_WindowInfo = info;

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
			WS_EX_APPWINDOW,
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
	}

	UIEvent WindowWin32::Impl::createMouseEvent(UINT message, WPARAM wParam, LPARAM lParam) {
		UIEvent event = UIEvent(UIEventType::MouseMove);
		MouseEventData& mouse = event.getMouseData();

		// Acquire mouse position
		POINT pt;
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);

		if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
			ScreenToClient(m_Handle, &pt);
		}

		mouse.position = { pt.x, pt.y };

		// Acquire mouse button states
		mouse.downButtons.left = (GET_KEYSTATE_WPARAM(wParam) & MK_LBUTTON) > 0;
		mouse.downButtons.middle = (GET_KEYSTATE_WPARAM(wParam) & MK_MBUTTON) > 0;
		mouse.downButtons.right = (GET_KEYSTATE_WPARAM(wParam) & MK_RBUTTON) > 0;

		if (message == WM_MOUSEWHEEL) {
			mouse.wheelDelta.y = GET_WHEEL_DELTA_WPARAM(wParam) * 10.0f / WHEEL_DELTA;
		}
		else if (message == WM_MOUSEHWHEEL) {
			mouse.wheelDelta.x = GET_WHEEL_DELTA_WPARAM(wParam) * 10.0f / WHEEL_DELTA;
		}

		// Check which mouse buttons caused the mouse event (if any)
		switch (message) {
		case WM_LBUTTONUP:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
			mouse.causeButtons.left = true;
			break;
		case WM_RBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
			mouse.causeButtons.right = true;
			break;
		case WM_MBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
			mouse.causeButtons.middle = true;
			break;
		case WM_MOUSEMOVE:
			if (m_MouseButtonEvent.getType() == UIEventType::MouseDown) {
				mouse.causeButtons = m_MouseButtonEvent.getMouseData().causeButtons;
			}
			break;
		}

		bool buttonPressed = mouse.downButtons.left || mouse.downButtons.middle || mouse.downButtons.right;

		// Set event types accordingly
		switch (message) {
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_XBUTTONUP:
			event.setType(UIEventType::MouseUp);
			event.getMouseData().clickCount = 0;

			if (!buttonPressed) {
				ReleaseCapture();
			}
			break;
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
			event.setType(UIEventType::MouseDown);
			event.getMouseData().clickCount = 2;
			break;
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_XBUTTONDOWN:
			event.setType(UIEventType::MouseDown);
			event.getMouseData().clickCount = 1;
			SetCapture(m_Handle);
			break;
		case WM_MOUSELEAVE:
		case WM_NCMOUSELEAVE:
		{
			event.setType(UIEventType::MouseExitWindow);
			m_TrackingMouseLeave = false;
		}
		break;
		case WM_MOUSEMOVE:
			// Hack: The Windows OS sends a WM_MOUSEMOVE message together with mouse down
			// flag when the window becomes the foreground window. Thus, we must also
			// check whether the mouse down event is the first since regaining the
			// foreground window focus. When handling WM_SETFOCUS we set the
			// m_EnteringWindow flag to 'true' which indicates this.
			// If that is the case, then we will not interpret that as a mouse-drag event
			// and then reset m_EnteringWindow to 'false'.
			event.setType((buttonPressed && !m_EnteringWindow) ? UIEventType::MouseDrag : UIEventType::MouseMove);

			if (m_EnteringWindow) {
				m_EnteringWindow = false;
			}

			break;
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			event.setType(UIEventType::MouseWheel);
			break;
		}

		if (!m_TrackingMouseLeave && message != WM_MOUSELEAVE && message != WM_NCMOUSELEAVE && message != WM_NCMOUSEMOVE) {
			TRACKMOUSEEVENT tme{};
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = m_Handle;

			TrackMouseEvent(&tme);
			m_TrackingMouseLeave = true;
		}

		// Store last occurrence of button press/release
		if (event.getType() == UIEventType::MouseDown ||
			event.getType() == UIEventType::MouseUp ||
			event.getType() == UIEventType::MouseExitWindow) {
			m_MouseButtonEvent = event;
		}

		return event;
	}

	/* ---------------------------------------------------------------------- */
	/*                         WindowWin32 Interface                          */
	/* ---------------------------------------------------------------------- */
	WindowWin32::WindowWin32(const WindowInfo& info) : IWindow() {
		m_Impl = new Impl();

		m_Impl->initialize(info);
	}

	WindowWin32::~WindowWin32() {
		delete m_Impl;
	}

	void WindowWin32::pollEvents() {
		MSG msg = {};

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				m_Impl->m_ShouldClose = true;
			}
			else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	void WindowWin32::show() {
		ShowWindow(m_Impl->m_Handle, SW_SHOW);
	}

	bool WindowWin32::shouldClose() {
		return m_Impl->m_ShouldClose;
	}

	int WindowWin32::getClientWidth() const {
		return m_Impl->m_ClientRect.right - m_Impl->m_ClientRect.left;
	}

	int WindowWin32::getClientHeight() const {
		return m_Impl->m_ClientRect.bottom - m_Impl->m_ClientRect.top;
	}

	int WindowWin32::getWindowWidth() const {
		return m_Impl->m_WindowRect.right - m_Impl->m_WindowRect.left;
	}

	int WindowWin32::getWindowHeight() const {
		return m_Impl->m_WindowRect.bottom - m_Impl->m_WindowRect.top;
	}

	void* WindowWin32::getHandle() {
		return (void*)m_Impl->m_Handle;
	}

}