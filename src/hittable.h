#pragma once

#include "math/sray_math.h"

struct HitData {
    Vec3 position{};
    Vec3 normal{};
    float t = 0.0;
};

class Hittable {
public:
    Hittable() = default;
    virtual ~Hittable() = default;

    virtual bool hit(const Ray& ray, float tMin, float tMax, HitData* const hitData) const = 0;
};
