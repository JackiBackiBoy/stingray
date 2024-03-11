#pragma once

#include "math/sray_math.h"

struct HitData;

class Material {
public:
    virtual ~Material() = default;

    virtual bool scatter(
        const Ray& rayIn,
        const HitData& hitData,
        Vec3* const attenuation,
        Ray* const rayScattered,
        uint32_t* seed) const = 0;
};

// Note: Also known as Lambertian material
class DiffuseMaterial final : public Material {
public:
    DiffuseMaterial(const Vec3& color) : albedo(color) {}

    Vec3 albedo{};

    bool scatter(
        const Ray& rayIn,
        const HitData& hitData,
        Vec3* const attenuation,
        Ray* const rayScattered,
        uint32_t* seed) const override;
};

class MetalMaterial final : public Material {
public:
    MetalMaterial(const Vec3& color, float f) : albedo(color), fuzz(f < 1.0f ? f : 1.0f) {}

    bool scatter(
        const Ray& rayIn,
        const HitData& hitData,
        Vec3* const attenuation,
        Ray* const rayScattered,
        uint32_t* seed) const override;

    Vec3 albedo{};
    float fuzz = 0.0f;
};

class DielectricMaterial final : public Material {
public:
    DielectricMaterial(float _refractionIndex) : refractionIndex(_refractionIndex) {}

    float refractionIndex = 0.0f;

    bool scatter(
        const Ray& rayIn,
        const HitData& hitData,
        Vec3* const attenuation,
        Ray* const rayScattered,
        uint32_t* seed) const override;
};
