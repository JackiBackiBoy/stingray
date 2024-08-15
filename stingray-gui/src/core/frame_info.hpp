#pragma once

#include <glm/glm.hpp>
#include "../math/quat.hpp"

namespace sr {
	struct Camera {
		glm::mat4 projectionMatrix = { 1.0f };
		glm::mat4 viewMatrix = { 1.0f };
		glm::vec3 position = { 0.0f, 0.0f, 0.0f };
		float verticalFOV = glm::radians(60.0f);
		quat orientation = {};
	};

	struct FrameInfo {
		Camera camera = {};
		uint32_t width = 0;
		uint32_t height = 0;
		float dt = 0.0f;
	};
}