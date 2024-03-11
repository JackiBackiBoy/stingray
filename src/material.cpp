#include "material.h"

#include "hittable.h"

/* Diffuse Material */
bool DiffuseMaterial::scatter(
    const Ray& rayIn,
    const HitData& hitData,
    Vec3* const attenuation,
    Ray* const rayScattered,
    uint32_t* seed) const {

    Vec3 scatterDir = hitData.normal + randomUnitVec3(seed);

    if (scatterDir.isNearZero()) {
        scatterDir = hitData.normal;
    }

    *rayScattered = { hitData.position, scatterDir };
    *attenuation = albedo;

    return true;
}

/* Metal Material */
bool MetalMaterial::scatter(
    const Ray& rayIn,
    const HitData& hitData,
    Vec3* const attenuation,
    Ray* const rayScattered,
    uint32_t* seed) const {

    Vec3 reflected = reflect(normalize(rayIn.dir), hitData.normal);
    *rayScattered = { hitData.position, reflected + fuzz * randomUnitVec3(seed) };
    *attenuation = albedo;

    return dot(rayScattered->dir, hitData.normal) > 0;
}

/* Dielectric Material */
bool DielectricMaterial::scatter(
    const Ray& rayIn,
    const HitData& hitData,
    Vec3* const attenuation,
    Ray* const rayScattered,
    uint32_t* seed) const {

    *attenuation = { 1.0f, 1.0f, 1.0f };
    const float refractionRatio = hitData.frontFace ? (1.0f / refractionIndex) : refractionIndex;

    const Vec3 unitDir = normalize(rayIn.dir);
    const float cosTheta = fmin(dot(-unitDir, hitData.normal), 1.0f);
    const float sinTheta = sqrtf(1.0f - cosTheta * cosTheta);

    const bool cannotRefract = refractionRatio * sinTheta > 1.0f;
    Vec3 direction{};

    if (cannotRefract || schlickReflectance(cosTheta, refractionRatio) > randomFloat(seed)) {
        direction = reflect(unitDir, hitData.normal);
    }
    else {
        direction = refract(unitDir, hitData.normal, refractionRatio);
    }

    *rayScattered = { hitData.position, direction };

    return true;
}
