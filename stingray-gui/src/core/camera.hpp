#pragma once

#include <array>

#include <glm/glm.hpp>
#include "../math/quat.hpp"

namespace sr {
	class Camera {
	public:
		Camera(
			const glm::vec3& position,
			const quat& orientation,
			float verticalFOV,
			float aspectRatio,
			float zNear,
			float zFar
		);
		~Camera() = default;

		struct Frustum {
			glm::vec3 corners[8] = {};

			std::array<glm::vec3, 4> getNearPlane();
			std::array<glm::vec3, 4> getFarPlane();

			Frustum getSubFrustum(int slices, int sliceIndex);
			static Frustum getFrustum(const glm::mat4& proj, const glm::mat4& view) noexcept;
		};

		void update() noexcept; // NOTE: Should ideally be called once per frame

		void setPosition(const glm::vec3& position) noexcept;
		void setOrientation(const quat& orientation) noexcept;
		void setVerticalFOV(float fov) noexcept;
		void setAspectRatio(float aspectRatio) noexcept;

		inline glm::vec3 getPosition() const noexcept { return m_Position; }
		inline quat getOrientation() const noexcept { return m_Orientation; }
		inline float getVerticalFOV() const noexcept { return m_VerticalFOV; }
		inline float getAspectRatio() const noexcept { return m_AspectRatio; }
		inline float getZNear() const noexcept { return m_ZNear; }
		inline float getZFar() const noexcept { return m_ZFar; }
		inline glm::mat4 getViewMatrix() const noexcept { return m_ViewMatrix; } // NOTE: Only updated when update() is called
		inline glm::mat4 getProjMatrix() const noexcept { return m_ProjMatrix; } // NOTE: Only updated when update() is called
		inline glm::mat4 getInvViewProjMatrix() const noexcept { return m_InvViewProjMatrix; } // NOTE: Only updated when update() is called
		inline glm::vec3 getRight() const noexcept { return m_Right; }
		inline glm::vec3 getUp() const noexcept { return m_Up; }
		inline glm::vec3 getForward() const noexcept { return m_Forward; }
		inline Frustum getFrustum() const noexcept { return m_Frustum; }

	private:
		void updateViewMatrix();
		void updateProjMatrix();
		void updateFrustum();

		bool m_UpdateViewMatrix = true;
		bool m_UpdateProjMatrix = true;

		glm::vec3 m_Position;
		quat m_Orientation;
		float m_VerticalFOV;
		float m_AspectRatio;
		float m_ZNear;
		float m_ZFar;

		glm::vec3 m_Right = { 1.0f, 0.0f, 0.0f };
		glm::vec3 m_Up = { 0.0f, 1.0f, 0.0f };
		glm::vec3 m_Forward = { 0.0f, 0.0f, 1.0f };

		glm::mat4 m_ViewMatrix = { 1.0f };
		glm::mat4 m_ProjMatrix = { 1.0f };
		glm::mat4 m_InvViewProjMatrix = { 1.0f };
		Frustum m_Frustum = {};

		static constexpr glm::vec3 LH_BASIS_RIGHT = { 1.0f, 0.0f, 0.0f };
		static constexpr glm::vec3 LH_BASIS_UP = { 0.0f, 1.0f, 1.0f };
		static constexpr glm::vec3 LH_BASIS_FORWARD = { 0.0f, 0.0f, 1.0f };
	};
}