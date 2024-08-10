#include "scene.hpp"


#include <glm/gtc/matrix_transform.hpp>
#include <cassert>

namespace sr {

	Scene::Scene() {
		m_SunLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		m_SunLight.direction = glm::normalize(glm::vec3(1.0f, 3.0f, -2.0f));
		updateLSMatrix();
	}

	Scene::~Scene() {
		for (size_t i = 0; i < m_Entities.size(); ++i) {
			delete m_Entities[i];
			m_Entities[i] = nullptr;
		}

		m_Entities.clear();
	}

	Entity* Scene::createEntity(const std::string& name) {
		assert(!m_EntityIndexLUT.contains(name));

		m_EntityIndexLUT.insert({ name, m_Entities.size() });
		m_Entities.push_back(new Entity());

		return m_Entities.back();
	}

	void Scene::setSunDirection(const glm::vec3& direction) {
		m_SunLight.direction = glm::normalize(direction);
		updateLSMatrix();
	}

	void Scene::setSunColor(const glm::vec4& color) {
		m_SunLight.color = color;
	}

	void Scene::updateLSMatrix() {
		const float projectionArea = 2.4f;
		const glm::mat4 lightProjection = glm::ortho(
			-projectionArea,
			projectionArea,
			-projectionArea,
			projectionArea,
			0.01f,
			10.0f
		);
		const glm::mat4 lightView = glm::lookAt(
			m_SunLight.direction * 3.0f,
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f }
		);

		m_SunLight.lightSpaceMatrix = lightProjection * lightView;
	}

}