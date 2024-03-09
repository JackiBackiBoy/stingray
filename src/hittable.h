#pragma once

#include "math/sray_math.h"

class Material;

struct HitData {
    Vec3 position{};
    Vec3 normal{};
    Material* material = nullptr;
    float t = 0.0f;
    bool frontFace = false;

    inline void setNormal(const Ray& ray, const Vec3& outwardNormal) {
        frontFace = dot(ray.dir, outwardNormal) < 0;
        normal = frontFace ? outwardNormal : -outwardNormal;
    }
};

class Hittable {
public:
    Hittable() = default;
    virtual ~Hittable() = default;

    virtual bool hit(const Ray& ray, float tMin, float tMax, HitData* const hitData) const = 0;
};
