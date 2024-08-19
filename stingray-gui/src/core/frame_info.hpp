#pragma once

#include "camera.hpp"
#include <glm/glm.hpp>
#include "../math/quat.hpp"

namespace sr {
	struct FrameInfo {
		Camera* camera = nullptr;
		uint32_t width = 0;
		uint32_t height = 0;
		float dt = 0.0f;
	};
}