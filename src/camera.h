#pragma once

#include "scene.h"
#include "math/sray_math.h"

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
    static void renderChunk(
        uint32_t* const imageBuffer,
        int yStart,
        int numRows,
        int imageWidth,
        const Scene& scene,
        Camera* camera
    );

    Vec3 computeColor(const Ray& ray, const Scene& scene, int depth) const;
    Ray generateRay(int x, int y) const;
    Vec3 pixelSampleSquare() const;
    Vec3 defocusDiskSample() const;

    float m_AspectRatio = 1.0f;
    Vec3 m_PixelDeltaX{};
    Vec3 m_PixelDeltaY{};
    Vec3 m_PixelTopLeft{};
    Vec3 m_U{}, m_V{}, m_W{}; // basis vectors
    Vec3 m_DefocusDiskX{};
    Vec3 m_DefocusDiskY{};
};
