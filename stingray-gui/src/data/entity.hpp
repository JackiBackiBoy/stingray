#pragma once

#include "model.hpp"
#include <glm/glm.hpp>

namespace sr {
	struct Entity {
		glm::vec3 position = { 0.0f, 0.0f, 0.0f };
		const Model* model = nullptr;
	};
}