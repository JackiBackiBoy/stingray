#pragma once

#include <cmath>

struct vec2 {
    float x = 0.0f;
    float y = 0.0f;

    vec2 operator+(const vec2& other) const {
        return { x + other.x, y + other.y };
    }

    vec2 operator-(const vec2& other) const {
        return { x - other.x, y - other.y };
    }

    vec2 operator*(float scalar) const {
        return { x * scalar, y * scalar };
    }
};

struct vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    vec3 operator+(const vec3& other) const {
        return { x + other.x, y + other.y, z + other.z };
    }

    vec3 operator-(const vec3& other) const {
        return { x - other.x, y - other.y, z - other.z };
    }

    vec3 operator*(float scalar) const {
        return { x * scalar, y * scalar, z * scalar };
    }

};

vec3 operator*(float scalar, vec3 u) {
    return u * scalar;
}

float dot(vec3 u, vec3 v) {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

vec3 normalize(vec3 u) {
    return (1.0f / sqrtf(u.x * u.x + u.y * u.y + u.z * u.z)) * u;
}
