#pragma once

#include "entity.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sr {
	class Scene {
	public:
		Scene();
		~Scene();

		Entity* createEntity(const std::string& name);

		void setSunColor(const glm::vec4& color);
		void setSunDirection(const glm::vec3& direction);

		inline const std::vector<Entity*>& getEntities() const { return m_Entities; }
		inline glm::vec4 getSunColor() const { return m_SunLight.color; }
		inline glm::vec3 getSunDirection() const { return m_SunLight.direction; }
		inline glm::mat4 getSunLSMatrix() const { return m_SunLight.lightSpaceMatrix; }
		inline DirectionLight& getSunLight() { return m_SunLight; }

	private:
		void updateLSMatrix();

		std::vector<Entity*> m_Entities = {};
		std::unordered_map<std::string, size_t> m_EntityIndexLUT = {};

		DirectionLight m_SunLight = {};
	};
}