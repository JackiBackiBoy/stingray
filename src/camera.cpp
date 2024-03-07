#include "camera.h"

#include <algorithm>
#include <limits>

Camera::Camera(int width, int height) :
    imageWidth(width), imageHeight(height) {

    m_AspectRatio = (float)imageWidth / imageHeight;
}

void Camera::render(const Scene& scene, uint32_t* const imageBuffer) {
    initialize();

    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            Vec3 pixelColor = { 0.0f, 0.0f, 0.0f };

            for (int sample = 0; sample < samplesPerPixel; ++sample) {
                const Ray ray = generateRay(x, y);
                pixelColor += computeColor(ray, scene);
            }

            float scale = 1.0f / samplesPerPixel;
            pixelColor *= scale;
            pixelColor.x = std::clamp(pixelColor.x, 0.0f, 0.999f);
            pixelColor.y = std::clamp(pixelColor.y, 0.0f, 0.999f);
            pixelColor.z = std::clamp(pixelColor.z, 0.0f, 0.999f);

            imageBuffer[y * imageWidth + x] = rgbToHex(pixelColor);
        }
    }
}

void Camera::initialize() {
    const float focalLength = 1.0f;
    const float viewportHeight = 2.0f;
    const float viewportWidth = viewportHeight * m_AspectRatio;

    const Vec3 viewportX = { viewportWidth, 0.0f, 0.0f };
    const Vec3 viewportY = { 0.0f, -viewportHeight, 0.0f };
    const Vec3 viewportTopLeft =
        position - Vec3{0.0f, 0.0f, focalLength} - viewportX / 2 - viewportY / 2;

    m_PixelDeltaX = viewportX / (float)imageWidth;
    m_PixelDeltaY = viewportY / (float)imageHeight;
    m_PixelTopLeft = viewportTopLeft + 0.5f * (m_PixelDeltaX + m_PixelDeltaY);
}

Vec3 Camera::computeColor(const Ray& ray, const Scene& scene) const {
    HitData hit{};

    if (scene.hit(ray, 0.0f, std::numeric_limits<float>::infinity(), &hit)) {
        const Vec3 bounceDir = randomHemisphereVec3(hit.normal);

        // Note: Due to floating point imprecisions we must add an
        // arbitrary value (epsilon) to the intersection point
        // in order to eliminate self-intersection
        const float epsilon = 0.0001f;
        const Vec3 intersectionPoint = hit.position + epsilon * hit.normal;

        return 0.5f * computeColor(Ray{intersectionPoint, bounceDir}, scene);
    }

    Vec3 unit_direction = normalize(ray.dir);
    const float a = 0.5 * (unit_direction.y + 1.0);
    return (1.0 - a)* Vec3{1.0, 1.0, 1.0} + a * Vec3{0.5, 0.7, 1.0};
}

Ray Camera::generateRay(int x, int y) const {
    const Vec3 pixelCenter = m_PixelTopLeft + (x * m_PixelDeltaX) + (y * m_PixelDeltaY);
    const Vec3 pixelSample = pixelCenter + pixelSampleSquare();

    const Vec3 rayOrigin = position;
    const Vec3 rayDir = pixelSample - rayOrigin;

    return { rayOrigin, rayDir };
}

Vec3 Camera::pixelSampleSquare() const {
    const float x = -0.5f + randomFloat();
    const float y = -0.5f + randomFloat();

    return x * m_PixelDeltaX + y * m_PixelDeltaY;
}
