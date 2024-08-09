#pragma once

#include <string>

namespace sr::utilities {
	std::wstring toWideString(const std::string& s);
	std::string wStringToString(const std::wstring wString);
}