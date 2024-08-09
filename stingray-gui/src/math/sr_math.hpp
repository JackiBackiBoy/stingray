#pragma once

namespace sr::math {
	inline float lerp(float a, float b, float t) {
		return a + (b - a) * t;
	}
}