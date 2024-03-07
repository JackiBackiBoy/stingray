#pragma once

#include <memory>
#include <vector>
#include "hittable.h"

struct Scene {
    std::vector<std::shared_ptr<Hittable>> objects;

    inline void add(std::shared_ptr<Hittable> object) {
        objects.push_back(object);
    }

    bool hit(const Ray& ray, float tMin, float tMax, HitData* const hitData) const;
};
