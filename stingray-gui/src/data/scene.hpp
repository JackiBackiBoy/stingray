#pragma once

#include "entity.hpp"
#include "../core/camera.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sr {
	struct PointLight {
		PointLight() = default;
		PointLight(const glm::vec4& color, const glm::vec3& position) :
			color(color), position(position) {}

		glm::vec4 color = {}; // NOTE: w-component is intensity
		glm::vec3 position = {};
		uint32_t pad1 = 0;
	};
	
	class Scene {
	public:
		Scene(Camera& camera);
		~Scene();

		void update();

		Entity* createEntity(const std::string& name);
		PointLight* createPointLight(const std::string& name, const glm::vec4& color, const glm::vec3& position);

		void setSunColor(const glm::vec4& color);
		void setSunDirection(const glm::vec3& direction);

		inline const std::vector<Entity*>& getEntities() const { return m_Entities; }
		inline const std::vector<PointLight*>& getPointLights() const { return m_PointLights; }
		inline glm::vec4 getSunColor() const { return m_SunLight.color; }
		inline glm::vec3 getSunDirection() const { return m_SunLight.direction; }
		inline glm::mat4 getSunViewMatrix() const { return m_SunLight.viewMatrix; }
		inline DirectionLight& getSunLight() { return m_SunLight; }

		static constexpr int MAX_POINT_LIGHTS = 32;

	private:
		void updateLSMatrix();

		Camera& m_Camera;

		std::vector<Entity*> m_Entities = {};
		std::vector<PointLight*> m_PointLights = {};
		std::unordered_map<std::string, size_t> m_EntityIndexLUT = {};
		std::unordered_map<std::string, size_t> m_PointLightIndexLUT = {};
		bool m_UpdateLSMatrix = false;

		DirectionLight m_SunLight = {};
	};
}