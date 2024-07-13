#pragma once

#include "hittable.h"

class Sphere final : public Hittable {
public:
    Sphere(Vec3 _position, float _radius, Material* _material) :
        position(_position), radius(_radius), material(_material) {}

    Vec3 position{};
    float radius = 0.5f;
    Material* material = nullptr;

    bool hit(const Ray& ray, float tMin, float tMax, HitData* const hitData) const override;
};
