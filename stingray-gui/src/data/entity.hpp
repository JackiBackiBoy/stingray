#pragma once

#include "model.hpp"
#include <glm/glm.hpp>
#include "../math/quat.hpp"

#define MAX_CASCADES 8

namespace sr {
	struct DirectionLight {
		glm::mat4 cascadeProjections[MAX_CASCADES] = {};
		glm::mat4 viewMatrix = { 1.0f };
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // NOTE: w is intensity
		float cascadeDistances[MAX_CASCADES] = {};
		glm::vec3 direction = {};
		uint32_t numCascades = 1;
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