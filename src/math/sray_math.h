#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    inline Vec3 operator-() const { return { -x, -y, -z }; }

    inline Vec3& operator+=(const Vec3& u) {
        x += u.x;
        y += u.y;
        z += u.z;

        return *this;
    }

    inline Vec3& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;

        return *this;
    }
};

/* Operator overloads */
// Vec2
inline Vec2 operator+(const Vec2& u, const Vec2& v) {
    return { u.x + v.x, u.y + v.y };
}

inline Vec2 operator-(const Vec2& u, const Vec2& v) {
    return { u.x - v.x, u.y - v.y };
}

inline Vec2 operator*(const Vec2& u, float scalar) {
    return { u.x * scalar, u.y * scalar };
}

inline Vec2 operator*(float scalar, const Vec2& u) {
    return u * scalar;
}

inline Vec2 operator/(const Vec2& u, float divisor) {
    return { u.x / divisor, u.y / divisor };
}

// Vec3
inline Vec3 operator+(const Vec3& u, const Vec3& v) {
    return { u.x + v.x, u.y + v.y, u.z + v.z };
}

inline Vec3 operator-(const Vec3& u, const Vec3& v) {
    return { u.x - v.x, u.y - v.y, u.z - v.z };
}

inline Vec3 operator*(const Vec3& u, float scalar) {
    return { u.x * scalar, u.y * scalar, u.z * scalar };
}

inline Vec3 operator*(float scalar, const Vec3& u) {
    return u * scalar;
}

inline Vec3 operator/(const Vec3& u, float divisor) {
    return { u.x / divisor, u.y / divisor, u.z / divisor };
}

inline float dot(const Vec3& u, const Vec3& v) {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

inline Vec3 normalize(const Vec3& u) {
    return (1.0f / sqrtf(u.x * u.x + u.y * u.y + u.z * u.z)) * u;
}

struct Ray {
    Vec3 origin{};
    Vec3 dir{};

    inline Vec3 at(float t) const {
        return origin + t * dir;
    }
};

/* Randomizers */
inline float randomFloat() {
    return rand() / (RAND_MAX + 1.0);
}

inline float randomFloat(float min, float max) {
    return min + (max - min) * randomFloat();
}

inline Vec3 randomVec3() {
    return { randomFloat(), randomFloat(), randomFloat() };
}

inline Vec3 randomVec3(float min, float max) {
    return { randomFloat(min, max), randomFloat(min, max), randomFloat(min, max) };
}

inline Vec3 randomUnitSphereVec3() {
    while (true) {
        const Vec3 u = randomVec3(-1.0f, 1.0f);

        if (dot(u, u) < 1.0f) {
            return u;
        }
    }
}

inline Vec3 randomUnitVec3() {
    return normalize(randomUnitSphereVec3());
}

inline Vec3 randomHemisphereVec3(const Vec3& normal) {
    Vec3 onUnitSphere = randomUnitVec3();

    if (dot(onUnitSphere, normal) > 0.0f) {
        return onUnitSphere;
    }

    return -onUnitSphere;
}

/* Converters */
inline uint32_t rgbToHex(Vec3 color) {
    const uint8_t r = (uint8_t)(color.x * 255.0f);
    const uint8_t g = (uint8_t)(color.y * 255.0f);
    const uint8_t b = (uint8_t)(color.z * 255.0f);

    return 0xff000000 | (b << 16) | (g << 8) | r;
}
