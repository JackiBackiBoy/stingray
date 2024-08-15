#pragma once

#include "entity.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sr {
	struct alignas(16) PointLight {
		PointLight() = default;
		PointLight(const glm::vec4& color, const glm::vec3& position) :
			color(color), position(position) {}

		glm::vec4 color = {}; // NOTE: w-component is intensity
		glm::vec3 position = {};
	};
	
	class Scene {
	public:
		Scene();
		~Scene();

		Entity* createEntity(const std::string& name);
		PointLight* createPointLight(const std::string& name, const glm::vec4& color, const glm::vec3& position);

		void setSunColor(const glm::vec4& color);
		void setSunDirection(const glm::vec3& direction);

		inline const std::vector<Entity*>& getEntities() const { return m_Entities; }
		inline const std::vector<PointLight*>& getPointLights() const { return m_PointLights; }
		inline glm::vec4 getSunColor() const { return m_SunLight.color; }
		inline glm::vec3 getSunDirection() const { return m_SunLight.direction; }
		inline glm::mat4 getSunLSMatrix() const { return m_SunLight.lightSpaceMatrix; }
		inline DirectionLight& getSunLight() { return m_SunLight; }

		static constexpr int MAX_POINT_LIGHTS = 32;

	private:
		void updateLSMatrix();

		std::vector<Entity*> m_Entities = {};
		std::vector<PointLight*> m_PointLights = {};
		std::unordered_map<std::string, size_t> m_EntityIndexLUT = {};
		std::unordered_map<std::string, size_t> m_PointLightIndexLUT = {};

		DirectionLight m_SunLight = {};
	};
}