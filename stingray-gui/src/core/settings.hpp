#pragma once

#include <glm/glm.hpp>

namespace sr {
	struct Settings {
		int verticalFOV = 60; // NOTE: in degrees
		bool enableShadows = true;
		bool enableAO = true;
		bool enableRayTracing = true;
		float ssmMinBias = 0.0001f;
		float ssmMaxBias = 0.005f;
		int numCascadeLevels = 3;
	};
}