#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace sr {
	Camera::Camera(const glm::vec3& position, const quat& orientation, float verticalFOV, float aspectRatio, float zNear, float zFar) :
		m_Position(position), m_Orientation(orientation),
		m_VerticalFOV(verticalFOV), m_AspectRatio(aspectRatio),
		m_ZNear(zNear), m_ZFar(zFar) {

		m_Right = quatRotateVector(m_Orientation, LH_BASIS_RIGHT);
		m_Up = quatRotateVector(m_Orientation, LH_BASIS_UP);
		m_Forward = quatRotateVector(m_Orientation, LH_BASIS_FORWARD);

		update();
	}

	void Camera::update() noexcept {
		if (m_UpdateViewMatrix) {
			updateViewMatrix();
		}

		if (m_UpdateProjMatrix) {
			updateProjMatrix();
		}

		if (m_UpdateViewMatrix || m_UpdateProjMatrix) {
			updateFrustum();
			m_InvViewProjMatrix = glm::inverse(m_ProjMatrix * m_ViewMatrix);
		}

		m_UpdateViewMatrix = false;
		m_UpdateProjMatrix = false;
	}

	void Camera::setPosition(const glm::vec3& position) noexcept {
		if (position == m_Position) {
			return;
		}

		m_UpdateViewMatrix = true;
		m_Position = position;
	}

	void Camera::setOrientation(const quat& orientation) noexcept {
		if (orientation == m_Orientation) {
			return;
		}

		m_UpdateViewMatrix = true;
		m_Orientation = orientation;

		m_Right = glm::normalize(quatRotateVector(m_Orientation, LH_BASIS_RIGHT));
		m_Up = glm::normalize(quatRotateVector(m_Orientation, LH_BASIS_UP));
		m_Forward = glm::normalize(quatRotateVector(m_Orientation, LH_BASIS_FORWARD));
	}

	void Camera::setVerticalFOV(float fov) noexcept {
		if (fov == m_VerticalFOV) {
			return;
		}

		m_UpdateProjMatrix = true;
		m_VerticalFOV = fov;
	}

	void Camera::setAspectRatio(float aspectRatio) noexcept {
		if (aspectRatio == m_AspectRatio) {
			return;
		}

		m_UpdateProjMatrix = true;
		m_AspectRatio = aspectRatio;
	}

	void Camera::updateViewMatrix() {
		m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
	}

	void Camera::updateProjMatrix() {
		m_ProjMatrix = glm::perspective(m_VerticalFOV, m_AspectRatio, m_ZNear, m_ZFar);
	}

	void Camera::updateFrustum() {
		const float tanHalfFOVTerm = tanf(m_VerticalFOV * 0.5f);
		
		// Near plane
		const glm::vec3 camToNearPlane = m_Position + m_Forward * m_ZNear;
		const float halfNearPlaneHeight = tanHalfFOVTerm * m_ZNear;
		const float halfNearPlaneWidth = halfNearPlaneHeight * m_AspectRatio;

		m_Frustum.corners[0] = camToNearPlane + m_Up * halfNearPlaneHeight - m_Right * halfNearPlaneWidth; // top-left
		m_Frustum.corners[1] = camToNearPlane + m_Up * halfNearPlaneHeight + m_Right * halfNearPlaneWidth; // top-right
		m_Frustum.corners[2] = camToNearPlane - m_Up * halfNearPlaneHeight + m_Right * halfNearPlaneWidth; // bottom-right
		m_Frustum.corners[3] = camToNearPlane - m_Up * halfNearPlaneHeight - m_Right * halfNearPlaneWidth; // bottom-left

		// Far plane
		const glm::vec3 camToFarPlane = m_Position + m_Forward * m_ZFar;
		const float halfFarPlaneWidth = tanHalfFOVTerm * m_ZFar;
		const float halfFarPlaneHeight = halfFarPlaneWidth * m_AspectRatio;

		m_Frustum.corners[4] = camToFarPlane + m_Up * halfFarPlaneHeight - m_Right * halfFarPlaneWidth; // top-left
		m_Frustum.corners[5] = camToFarPlane + m_Up * halfFarPlaneHeight + m_Right * halfFarPlaneWidth; // top-right
		m_Frustum.corners[6] = camToFarPlane - m_Up * halfFarPlaneHeight + m_Right * halfFarPlaneWidth; // bottom-right
		m_Frustum.corners[7] = camToFarPlane - m_Up * halfFarPlaneHeight - m_Right * halfFarPlaneWidth; // bottom-left
	}

	std::array<glm::vec3, 4> Camera::Frustum::getNearPlane() {
		return { corners[0], corners[1], corners[2], corners[3] };
	}

	std::array<glm::vec3, 4> Camera::Frustum::getFarPlane() {
		return { corners[4], corners[5], corners[6], corners[7] };
	}

	Camera::Frustum Camera::Frustum::getSubFrustum(int slices, int sliceIndex) {
		assert(sliceIndex >= 0 && sliceIndex < slices);

		if (slices == 1) {
			return *this;
		}

		const std::array<glm::vec3, 4> nearToFarDirs = {
			corners[4] - corners[0], // top-left dir
			corners[5] - corners[1], // top-right dir
			corners[6] - corners[2], // bottom-right dir
			corners[7] - corners[3], // bottom-left dir
		};

		const float invSlices = 1.0f / slices;
		const float tNear = sliceIndex * invSlices;
		const float tFar = tNear + invSlices;

		return Frustum {
			// Near plane
			corners[0] + tNear * nearToFarDirs[0],
			corners[1] + tNear * nearToFarDirs[1],
			corners[2] + tNear * nearToFarDirs[2],
			corners[3] + tNear * nearToFarDirs[3],

			// Far plane
			corners[0] + tFar * nearToFarDirs[0],
			corners[1] + tFar * nearToFarDirs[1],
			corners[2] + tFar * nearToFarDirs[2],
			corners[3] + tFar * nearToFarDirs[3],
		};
	}

	Camera::Frustum Camera::Frustum::getFrustum(const glm::mat4& proj, const glm::mat4& view) noexcept {
		const glm::mat4 invProjView = glm::inverse(proj * view);

		std::array<glm::vec4, 8> corners = {
			invProjView * glm::vec4(-1.0f,  1.0f, 0.0f, 1.0f),
			invProjView * glm::vec4(1.0f,  1.0f, 0.0f, 1.0f),
			invProjView * glm::vec4(1.0f, -1.0f, 0.0f, 1.0f),
			invProjView * glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f),
			invProjView * glm::vec4(-1.0f,  1.0f, 1.0f, 1.0f),
			invProjView * glm::vec4(1.0f,  1.0f, 1.0f, 1.0f),
			invProjView * glm::vec4(1.0f, -1.0f, 1.0f, 1.0f),
			invProjView * glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f)
		};

		// Perspective divide
		for (size_t i = 0; i < 8; ++i) {
			corners[i] /= corners[i].w;
		}

		return Frustum {
			corners[0],
			corners[1],
			corners[2],
			corners[3],
			corners[4],
			corners[5],
			corners[6],
			corners[7]
		};
	}

}