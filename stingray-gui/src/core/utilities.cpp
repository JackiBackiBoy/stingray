#include "utilities.hpp"

namespace sr::utilities {

	std::wstring toWideString(const std::string& s) {
		std::wstring ws(s.size(), L' '); // overestimate number of code points

		// TODO: This is unsafe on Windows, we will have to use mbstowcs_s in the future
		ws.resize(std::mbstowcs(&ws[0], s.c_str(), s.size())); // shrink to fit

		return ws;
	}

}