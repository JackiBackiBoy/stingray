#include "window_win32.hpp"

#include "../core/utilities.hpp"
#include "../input/rawinput.hpp"

namespace sr {

	WindowWin32::WindowWin32(const std::string& title, int width, int height) :
		IWindow() {

		std::wstring wTitle = sr::utilities::toWideString(title);

		const WNDCLASSEX wndClass = {
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
		RegisterClassEx(&wndClass);

		RECT windowRect = { 0, 0, width, height }; // set desired client area
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		m_Handle = CreateWindowEx(
			0,
			wndClass.lpszClassName,
			wndClass.lpszClassName,
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			nullptr,
			nullptr,
			wndClass.hInstance,
			nullptr
		);
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