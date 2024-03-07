#pragma once

#include "hittable.h"

struct Material {
    Vec3 color{}; // range 0-1 per channel (RGB)
};

class Sphere final : public Hittable {
public:
    Sphere(Vec3 pos, float rad) : position(pos), radius(rad) {}

    Vec3 position{};
    float radius = 0.5f;
    //Material material{};

    bool hit(const Ray& ray, float tMin, float tMax, HitData* const hitData) const override;
};
