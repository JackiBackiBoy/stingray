#pragma once

#include "scene.h"
#include "math/sray_math.h"

class Camera {
public:
    Camera(int width, int height);
    ~Camera() {};

    Vec3 position = { 0.0f, 0.0f, 0.0f };
    int imageWidth = 128;
    int imageHeight = 128;
    int samplesPerPixel = 100;
    int maxDepth = 10;

    void render(const Scene& scene, uint32_t* const imageBuffer);

private:
    void initialize();
    Vec3 computeColor(const Ray& ray, const Scene& scene) const;
    Ray generateRay(int x, int y) const;
    Vec3 pixelSampleSquare() const;

    float m_AspectRatio = 1.0f;
    Vec3 m_PixelDeltaX{};
    Vec3 m_PixelDeltaY{};
    Vec3 m_PixelTopLeft{};
};
