#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>

/* Constants */
constexpr float Pi = 3.141592653589793238462643383279502884e+00F;
constexpr float PiOver180 = Pi / 180.0f;
constexpr float epsilon = 0.000001f;

/* Vectors */
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

    inline float length() const { return sqrtf(x*x + y*y + z*z); }

    inline bool isNearZero() const {
        return (x < epsilon) && (y < epsilon) && (z < epsilon);
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

// Note: Component-wise multiplication
inline Vec3 operator*(const Vec3& u, const Vec3& v) {
    return { u.x * v.x, u.y * v.y, u.z * v.z };
}

inline Vec3 operator/(const Vec3& u, float divisor) {
    return { u.x / divisor, u.y / divisor, u.z / divisor };
}

inline float dot(const Vec3& u, const Vec3& v) {
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

inline Vec3 cross(const Vec3& u, const Vec3& v) {
    return {
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x
    };
}

inline Vec3 normalize(const Vec3& u) {
    return (1.0f / sqrtf(u.x * u.x + u.y * u.y + u.z * u.z)) * u;
}

inline Vec3 reflect(const Vec3& u, const Vec3& n) {
    return u - 2.0f * dot(u, n) * n;
}

inline Vec3 refract(const Vec3& uv, const Vec3& n, float etaiOverEtat) {
    const float cosTheta = fmin(dot(-uv, n), 1.0f);
    const Vec3 rOutPerpendicular = etaiOverEtat * (uv + cosTheta * n);
    const Vec3 rOutParallel = -sqrtf(fabs(1.0f - dot(rOutPerpendicular, rOutPerpendicular))) * n;

    return rOutPerpendicular + rOutParallel;
}

struct Ray {
    Vec3 origin{};
    Vec3 dir{};

    inline Vec3 at(float t) const {
        return origin + t * dir;
    }
};

/* Randomizers */
inline uint32_t xorShift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

inline float randomFloat(uint32_t* state) {
    return xorShift32(state) / 4294967296.0f;
}

inline float randomFloat(float min, float max, uint32_t* state) {
    return min + (max - min) * randomFloat(state);
}

inline Vec3 randomVec3(uint32_t* state) {
    return { randomFloat(state), randomFloat(state), randomFloat(state) };
}

inline Vec3 randomVec3(float min, float max, uint32_t* state) {
    return {
        randomFloat(min, max, state),
        randomFloat(min, max, state),
        randomFloat(min, max, state)
    };
}

inline Vec3 randomUnitSphereVec3(uint32_t* state) {
    while (true) {
        const Vec3 u = randomVec3(-1.0f, 1.0f, state);

        if (dot(u, u) < 1.0f) {
            return u;
        }
    }
}

inline Vec3 randomUnitVec3(uint32_t* state) {
    return normalize(randomUnitSphereVec3(state));
}

inline Vec3 randomHemisphereVec3(const Vec3& normal, uint32_t* state) {
    Vec3 onUnitSphere = randomUnitVec3(state);

    if (dot(onUnitSphere, normal) > 0.0f) {
        return onUnitSphere;
    }

    return -onUnitSphere;
}

inline Vec3 randomVec3InUnitDisk(uint32_t* state) {
    while (true) {
        const Vec3 p = {
            randomFloat(-1.0f, 1.0f, state),
            randomFloat(-1.0f, 1.0f, state),
            0.0f
        };

        if (dot(p, p) < 1.0f) {
            return p;
        }
    }
}

/* Converters */
inline uint32_t rgbToHex(Vec3 color) {
    const uint8_t r = (uint8_t)(color.x * 255.0f);
    const uint8_t g = (uint8_t)(color.y * 255.0f);
    const uint8_t b = (uint8_t)(color.z * 255.0f);

    return 0xff000000 | (b << 16) | (g << 8) | r;
}

inline float toRadians(float deg) {
    return PiOver180 * deg;
}

/* Color Spaces */
inline float linearToGamma(float linearComponent) {
    return sqrtf(linearComponent);
}

/* Approximations */
inline float schlickReflectance(float cosine, float refractionIndex) {
    float r0 = (1.0f - refractionIndex) / (1.0f + refractionIndex);
    r0 *= r0;

    return r0 + (1.0f - r0) * powf((1.0f - cosine), 5.0f);
}
