#pragma once

#include <glm/glm.hpp>

namespace sr {
	struct Settings {
		int verticalFOV = 60; // NOTE: in degrees
		bool enableShadows = true;
		bool enableAO = true;
		bool enableRayTracing = true;
	};
}