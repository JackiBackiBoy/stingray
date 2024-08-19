#include "scene.hpp"


#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cassert>

namespace sr {

	Scene::Scene(Camera& camera) : m_Camera(camera) {
		m_SunLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		m_SunLight.direction = glm::normalize(glm::vec3(1.0f, 3.0f, -2.0f));
		updateLSMatrix();
	}

	Scene::~Scene() {
		for (size_t i = 0; i < m_Entities.size(); ++i) {
			delete m_Entities[i];
			m_Entities[i] = nullptr;
		}

		for (size_t i = 0; i < m_PointLights.size(); ++i) {
			delete m_PointLights[i];
			m_PointLights[i] = nullptr;
		}

		m_Entities.clear();
		m_PointLights.clear();
	}

	void Scene::update() {
		updateLSMatrix();
	}

	Entity* Scene::createEntity(const std::string& name) {
		assert(!m_EntityIndexLUT.contains(name));

		m_EntityIndexLUT.insert({ name, m_Entities.size() });
		m_Entities.push_back(new Entity());

		return m_Entities.back();
	}

	PointLight* Scene::createPointLight(const std::string& name, const glm::vec4& color, const glm::vec3& position) {
		assert(!m_PointLightIndexLUT.contains(name) && m_PointLights.size() < MAX_POINT_LIGHTS);

		m_PointLightIndexLUT.insert({ name, m_PointLights.size() });

		PointLight* pointLight = new PointLight(color, position);
		m_PointLights.push_back(pointLight);

		return pointLight;
	}

	void Scene::setSunDirection(const glm::vec3& direction) {
		m_SunLight.direction = glm::normalize(direction);
		m_UpdateLSMatrix = true;
	}

	void Scene::setSunColor(const glm::vec4& color) {
		m_SunLight.color = color;
	}

	void Scene::updateLSMatrix() {
		// Tight projection fitting
		Camera::Frustum frustum = m_Camera.getFrustum();

		glm::vec3 frustumCenter = { 0.0f, 0.0f, 0.0f };
		for (size_t i = 0; i < 8; ++i) {
			frustumCenter += frustum.corners[i];
		}

		frustumCenter /= 8;

		const glm::mat4 lightView = glm::lookAt(
			frustumCenter + m_SunLight.direction * 3.0f,
			frustumCenter,
			{ 0.0f, 1.0f, 0.0f }
		);

		//// Project each near-plane corner on the XZ-plane (centered at (0, 0, 0))
		//// in the direction of the corresponding far-plane corner.
		//std::array<glm::vec3, 4> nearPlaneProjDirs = {
		//	glm::normalize(farPlane[0] - nearPlane[0]),
		//	glm::normalize(farPlane[1] - nearPlane[1]),
		//	glm::normalize(farPlane[2] - nearPlane[2]),
		//	glm::normalize(farPlane[3] - nearPlane[3])
		//};

		//std::array<glm::vec4, 8> lightViewFrustum = {
		//	lightView * glm::vec4(frustum.corners[0], 1.0f),
		//	lightView * glm::vec4(frustum.corners[1], 1.0f),
		//	lightView * glm::vec4(frustum.corners[2], 1.0f),
		//	lightView * glm::vec4(frustum.corners[3], 1.0f),
		//	lightView * glm::vec4(frustum.corners[4], 1.0f),
		//	lightView * glm::vec4(frustum.corners[5], 1.0f),
		//	lightView * glm::vec4(frustum.corners[6], 1.0f),
		//	lightView * glm::vec4(frustum.corners[7], 1.0f)
		//};

		//// Given since the parameter t is given by t = -P.y / ProjDir.y
		//std::vector<glm::vec4> projectedNearPlane(4);

		//// Perform the projection
		//for (size_t i = 0; i < 4; ++i) {
		//	// Parameter t is given by solving for t in the plane equation: P.y + t * ProjDir.y = 0
		//	// NOTE: To prevent projection coords too far away (when ProjDir.y approaches 0)
		//	// and coords too close to the camera, we will clamp the depth between ZNear and ZFar.
		//	glm::vec3 projDir = nearPlaneProjDirs[i];
		//	const float epsilon = 0.0001f;
		//	float projDirY = nearPlaneProjDirs[i].y;

		//	// Prevent division by zero
		//	if (fabs(projDirY) < epsilon) {
		//		projDirY = (projDirY < 0.0f) ? -epsilon : epsilon;
		//	}

		//	float t = -nearPlane[i].y / projDirY;

		//	if (fabs(t) > m_Camera.getZFar()) {
		//		// When this occurs, it means that we are attempting to project a
		//		// near plane corner too far away. Thus, will resort to simply
		//		// use the far plane corners

		//		projectedNearPlane[i] = lightView * glm::vec4(nearPlane[i], 1.0f);
		//		projectedNearPlane.push_back(lightView * glm::vec4(farPlane[i], 1.0f));

		//		continue;
		//	}
		//	else if (fabs(t) < m_Camera.getZNear()) {
		//		t = (t < 0.0f) ? -m_Camera.getZNear() : m_Camera.getZNear();
		//	}

		//	projectedNearPlane[i] = lightView * glm::vec4(nearPlane[i] + t * projDir, 1.0f);
		//}

		//glm::vec3 min = glm::vec3(INFINITY);
		//glm::vec3 max = glm::vec3(-INFINITY);

		//for (size_t i = 0; i < lightViewFrustum.size(); ++i) {
		//	if (lightViewFrustum[i].x < min.x) {
		//		min.x = lightViewFrustum[i].x;
		//	}
		//	if (lightViewFrustum[i].y < min.y) {
		//		min.y = lightViewFrustum[i].y;
		//	}
		//	if (lightViewFrustum[i].z < min.z) {
		//		min.z = lightViewFrustum[i].z;
		//	}

		//	if (lightViewFrustum[i].x > max.x) {
		//		max.x = lightViewFrustum[i].x;
		//	}
		//	if (lightViewFrustum[i].y > max.y) {
		//		max.y = lightViewFrustum[i].y;
		//	}
		//	if (lightViewFrustum[i].z > max.z) {
		//		max.z = lightViewFrustum[i].z;
		//	}
		//}

		//const glm::mat4 lightProj = glm::ortho(min.x, max.x, min.y, max.y, min.z, max.z);

		m_SunLight.viewMatrix = lightView;
	}

}