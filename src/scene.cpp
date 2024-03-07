#include "scene.h"

bool Scene::hit(const Ray& ray, float tMin, float tMax, HitData* const hitData) const {
    HitData tempHitData{};
    bool anyHit = false;
    float closestT = tMax;

    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i]->hit(ray, tMin, closestT, &tempHitData)) {
            anyHit = true;
            closestT = tempHitData.t;
            *hitData = tempHitData;
        }
    }

    return anyHit;
}
