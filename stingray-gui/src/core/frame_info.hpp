#pragma once

#include <glm/glm.hpp>
#include "../math/quat.hpp"

namespace sr {
	struct Camera {
		glm::mat4 projectionMatrix = { 1.0f };
		glm::mat4 viewMatrix = { 1.0f };
		glm::vec3 position = { 0.0f, 0.0f, 0.0f };
		quat orientation = {};
	};

	struct FrameInfo {
		Camera camera = {};
		bool cameraMoved = false;
		uint32_t width = 0;
		uint32_t height = 0;
		float dt = 0.0f;
	};
}