#include "camera.h"
#include "material.h"

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
                pixelColor += computeColor(ray, scene, maxDepth);
            }

            const float scale = 1.0f / samplesPerPixel;
            pixelColor *= scale;

            // Linear to Gamma space transform
            pixelColor.x = linearToGamma(pixelColor.x);
            pixelColor.y = linearToGamma(pixelColor.y);
            pixelColor.z = linearToGamma(pixelColor.z);

            pixelColor.x = std::clamp(pixelColor.x, 0.0f, 0.999f);
            pixelColor.y = std::clamp(pixelColor.y, 0.0f, 0.999f);
            pixelColor.z = std::clamp(pixelColor.z, 0.0f, 0.999f);

            imageBuffer[y * imageWidth + x] = rgbToHex(pixelColor);
        }
    }
}

void Camera::initialize() {
    const float h = tanf(verticalFOV * 0.5f);
    const float viewportHeight = 2.0f * h * focusDistance;
    const float viewportWidth = viewportHeight * m_AspectRatio;

    m_W = normalize(position - lookAt);
    m_U = normalize(cross(up, m_W));
    m_V = cross(m_W, m_U);

    const Vec3 viewportX = viewportWidth * m_U;
    const Vec3 viewportY = viewportHeight * -m_V;
    const Vec3 viewportTopLeft =
        position - (focusDistance * m_W) - 0.5f * (viewportX + viewportY);

    m_PixelDeltaX = viewportX / (float)imageWidth;
    m_PixelDeltaY = viewportY / (float)imageHeight;
    m_PixelTopLeft = viewportTopLeft + 0.5f * (m_PixelDeltaX + m_PixelDeltaY);

    const float defocusRadius = focusDistance * tanf(defocusAngle * 0.5f);
    m_DefocusDiskX = m_U * defocusRadius;
    m_DefocusDiskY = m_V * defocusRadius;
}

Vec3 Camera::computeColor(const Ray& ray, const Scene& scene, int depth) const {
    HitData hit{};

    if (depth <= 0) {
        return { 0.0f, 0.0f, 0.0f };
    }

    // Note: Due to floating point rounding errors, we must add an
    // arbitrary value (epsilon) to the intersection point
    // in order to eliminate self-intersection
    const float epsilon = 0.001f;
    if (scene.hit(ray, epsilon, std::numeric_limits<float>::infinity(), &hit)) {
        Ray scattered{};
        Vec3 attenuation{};

        if (hit.material->scatter(ray, hit, &attenuation, &scattered)) {
            return attenuation * computeColor(scattered, scene, depth - 1);
        }

        return { 0.0f, 0.0f, 0.0f };
    }

    const Vec3 unit_direction = normalize(ray.dir);
    const float a = 0.5 * (unit_direction.y + 1.0);
    return (1.0 - a)* Vec3{1.0, 1.0, 1.0} + a * Vec3{0.5, 0.7, 1.0};
}

Ray Camera::generateRay(int x, int y) const {
    const Vec3 pixelCenter = m_PixelTopLeft + (x * m_PixelDeltaX) + (y * m_PixelDeltaY);
    const Vec3 pixelSample = pixelCenter + pixelSampleSquare();

    const Vec3 rayOrigin = (defocusAngle > 0.0f) ? defocusDiskSample() : position;
    const Vec3 rayDir = pixelSample - rayOrigin;

    return { rayOrigin, rayDir };
}

Vec3 Camera::pixelSampleSquare() const {
    const float x = -0.5f + randomFloat();
    const float y = -0.5f + randomFloat();

    return x * m_PixelDeltaX + y * m_PixelDeltaY;
}

Vec3 Camera::defocusDiskSample() const {
    const Vec3 p = randomVec3InUnitDisk();

    return position + (p.x * m_DefocusDiskX) + (p.y * m_DefocusDiskY);
}

