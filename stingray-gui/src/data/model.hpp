#pragma once

#include "../rendering/graphics.hpp"
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace sr {
	struct Mesh {
		uint32_t numIndices = 0;
		uint32_t baseVertex = 0;
		uint32_t baseIndex = 0;
	};

	struct ModelVertex {
		glm::vec3 position = {};
	};

	struct Model {
		std::vector<Mesh> meshes = {};
		std::vector<ModelVertex> vertices = {};
		std::vector<uint32_t> indices = {};

		Buffer vertexBuffer = {};
		Buffer indexBuffer = {};
	};
}