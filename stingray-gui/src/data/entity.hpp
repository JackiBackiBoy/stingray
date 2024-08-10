#pragma once

#include "model.hpp"
#include <glm/glm.hpp>
#include "../math/quat.hpp"

namespace sr {
	struct DirectionLight {
		glm::mat4 lightSpaceMatrix = { 1.0f };
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // NOTE: w is intensity
		glm::vec3 direction = {};
	};

	struct Entity {
		glm::vec3 position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
		quat orientation = {};
		const Model* model = nullptr;

		// NOTE: Temporary attributes that only exist for testing purposes
		glm::vec3 color = { 1.0f, 1.0f, 1.0f };
		float roughness = 1.0f;
	};
}