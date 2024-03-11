#pragma once

#include "scene.h"
#include "math/sray_math.h"

#include <atomic>

class Camera {
public:
    Camera(int width, int height);
    ~Camera() {};

    Vec3 position = { 0.0f, 0.0f, -1.0f };
    Vec3 lookAt = { 0.0f, 0.0f, 0.0f };
    Vec3 up = { 0.0f, 1.0f, 0.0f };
    float verticalFOV = Pi * 0.5f;
    float defocusAngle = 0.0f;
    float focusDistance = 10.0f;

    int imageWidth = 128;
    int imageHeight = 128;
    int samplesPerPixel = 100;
    int maxDepth = 10;
    int numThreads = 1;

    void render(const Scene& scene, uint32_t* const imageBuffer);

private:
    void initialize();
    void renderChunk(
        uint32_t* const imageBuffer,
        int imageWidth,
        int imageHeight,
        const Scene& scene
    );

    Vec3 computeColor(const Ray& ray, const Scene& scene, int depth, uint32_t* seed) const;
    Ray generateRay(int x, int y, uint32_t* seed) const;
    Vec3 pixelSampleSquare(uint32_t* seed) const;
    Vec3 defocusDiskSample(uint32_t* seed) const;

    std::atomic<int> m_NextRow{ 0 };
    float m_AspectRatio = 1.0f;
    Vec3 m_PixelDeltaX{};
    Vec3 m_PixelDeltaY{};
    Vec3 m_PixelTopLeft{};
    Vec3 m_U{}, m_V{}, m_W{}; // basis vectors
    Vec3 m_DefocusDiskX{};
    Vec3 m_DefocusDiskY{};
};
