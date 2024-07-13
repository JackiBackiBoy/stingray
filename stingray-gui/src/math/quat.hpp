#pragma once

#pragma once

#include <cmath>
#include <glm/glm.hpp>

namespace sr {
	struct quat {
		float w = 1.0f; // scalar
		float x = 0.0f; // vector-part 1
		float y = 0.0f; // vector-part 2
		float z = 0.0f; // vector-part 3

		inline float norm() const {
			return sqrtf(w * w + x * x + y * y + z * z);
		}
	};

	inline quat operator*(const quat& q1, const quat& q2) {
		return {
			q1.w * q2.w - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z),
			(q1.y * q2.z - q1.z * q2.y) + q1.w * q2.x + q2.w * q1.x,
			(q1.z * q2.x - q1.x * q2.z) + q1.w * q2.y + q2.w * q1.y,
			(q1.x * q2.y - q1.y * q2.x) + q1.w * q2.z + q2.w * q1.z
		};
	}

	inline quat quatConjugate(const quat& q) {
		return {
			q.w,
			-q.x,
			-q.y,
			-q.z
		};
	}

	inline quat quatInverse(const quat& q) {
		const float invNormSquared = 1.0f / (q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);

		// NOTE: Implicitly using quaternion conjugate
		return {
			q.w * invNormSquared,
			-q.x * invNormSquared,
			-q.y * invNormSquared,
			-q.z * invNormSquared
		};
	}

	inline glm::vec3 quatRotateVector(const quat& q, const glm::vec3& v) {
		const quat tempQuaternion = {
				.w = 0.0f,
				.x = v.x,
				.y = v.y,
				.z = v.z
		};

		const quat result = q * tempQuaternion * quatConjugate(q);
		return glm::vec3(result.x, result.y, result.z);
	}

	inline quat quatFromAxisAngle(const glm::vec3& axis, float angle) {
		const glm::vec3 normAxis = normalize(axis);
		const float sineTerm = sinf(angle * 0.5f);

		return {
			cosf(angle * 0.5f),
			normAxis.x * sineTerm,
			normAxis.y * sineTerm,
			normAxis.z * sineTerm
		};
	}

	inline quat quatFromEuler(float yaw, float pitch, float roll) {
		const quat qYaw = {
			.w = cosf(yaw / 2),
			.x = 0.0f,
			.y = sinf(yaw / 2),
			.z = 0.0f
		};

		const quat qPitch = {
			.w = cosf(pitch / 2),
			.x = sinf(pitch / 2),
			.y = 0.0f,
			.z = 0.0f
		};

		const quat qRoll = {
			.w = cosf(roll / 2),
			.x = 0.0f,
			.y = 0.0f,
			.z = sinf(roll / 2)
		};

		return qYaw * qPitch * qRoll;
	}

	inline quat quatNormalize(const quat& q) {
		const float invNorm = 1.0f / q.norm();

		return {
			q.w * invNorm,
			q.x * invNorm,
			q.y * invNorm,
			q.z * invNorm
		};
	}

	inline glm::mat4 quatToMat4(const quat& q) {
		const glm::vec4 col0 = {
			1.0f - 2.0f * (q.y * q.y + q.z * q.z),
			2.0f * (q.x * q.y + q.w * q.z),
			2.0f * (q.x * q.z - q.w * q.y),
			0.0f
		};

		const glm::vec4 col1 = {
			2.0f * (q.x * q.y - q.w * q.z),
			1.0f - 2.0f * (q.x * q.x + q.z * q.z),
			2.0f * (q.y * q.z + q.w * q.x),
			0.0f
		};

		const glm::vec4 col2 = {
			2.0f * (q.x * q.z + q.w * q.y),
			2.0f * (q.y * q.z - q.w * q.x),
			1.0f - 2.0f * (q.x * q.x + q.y * q.y),
			0.0f
		};

		return glm::mat4(col0, col1, col2, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	}

	inline glm::mat3 quatToMat3(const quat& q) {
		const glm::vec3 col0 = {
			1.0f - 2.0f * (q.y * q.y + q.z * q.z),
			2.0f * (q.x * q.y + q.w * q.z),
			2.0f * (q.x * q.z - q.w * q.y)
		};

		const glm::vec3 col1 = {
			2.0f * (q.x * q.y - q.w * q.z),
			1.0f - 2.0f * (q.x * q.x + q.z * q.z),
			2.0f * (q.y * q.z + q.w * q.x)
		};

		const glm::vec3 col2 = {
			2.0f * (q.x * q.z + q.w * q.y),
			2.0f * (q.y * q.z - q.w * q.x),
			1.0f - 2.0f * (q.x * q.x + q.y * q.y)
		};

		return glm::mat3(col0, col1, col2);
	}
}