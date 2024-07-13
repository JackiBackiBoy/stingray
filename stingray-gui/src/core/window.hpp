#pragma once

#if defined(SR_WINDOWS)
	#include "../windows/window_win32.hpp"
#elif defined(SR_MACOS)
	#include "../macos/window_macos.hpp"
#endif