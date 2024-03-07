#include "sphere.h"

bool Sphere::hit(const Ray& ray, float tMin, float tMax, HitData* const hitData) const {
    const Vec3 oc = ray.origin - position;
    const float a = dot(ray.dir, ray.dir);
    const float bHalf = dot(ray.dir, oc);
    const float c = dot(oc, oc) - radius * radius;
    const float discriminant = bHalf * bHalf - a * c;

    if (discriminant < 0.0f) { // sphere was not hit
        return false;
    }

    const float invDenom = 1.0f / a;
    const float sqrtTerm = sqrtf(discriminant);
    float root = (-bHalf - sqrtTerm) * invDenom;

    if (root <= tMin || tMax <= root) {
        root = (-bHalf + sqrtTerm) * invDenom;

        if (root <= tMin || tMax <= root) {
            return false;
        }
    }

    hitData->t = root;
    hitData->position = ray.at(root);
    hitData->normal = (hitData->position - position) * (1.0f / radius);

    return true;
}
