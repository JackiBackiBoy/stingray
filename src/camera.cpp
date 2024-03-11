#include "camera.h"
#include "material.h"

#include <algorithm>
#include <limits>
#include <thread>

Camera::Camera(int width, int height) :
    imageWidth(width), imageHeight(height) {

    m_AspectRatio = (float)imageWidth / imageHeight;
}

void Camera::render(const Scene& scene, uint32_t* const imageBuffer) {
    initialize();

    std::vector<std::thread> threads(numThreads);

    for (size_t i = 0; i < numThreads; ++i) {
        threads[i] = std::thread(
            &Camera::renderChunk,
            this,
            imageBuffer,
            imageWidth,
            imageHeight,
            scene
        );
    }

    // Wait for all threads to finish execution
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
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

void Camera::renderChunk(
    uint32_t* const imageBuffer,
    int imageWidth,
    int imageHeight,
    const Scene& scene) {

    uint32_t seed = std::hash<std::thread::id>()(std::this_thread::get_id()) + 1;

    while (true) {
        // Determine which row the thread should process next
        const int row = m_NextRow.fetch_add(1);

        if (row >= imageHeight) {
            break;
        }

        for (int x = 0; x < imageWidth; ++x) {
            Vec3 pixelColor = { 0.0f, 0.0f, 0.0f };

            for (int sample = 0; sample < samplesPerPixel; ++sample) {
                const Ray ray = generateRay(x, row, &seed);
                pixelColor += computeColor(ray, scene, maxDepth, &seed);
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

            imageBuffer[row * imageWidth + x] = rgbToHex(pixelColor);
        }
    }
}

Vec3 Camera::computeColor(const Ray& ray, const Scene& scene, int depth, uint32_t* seed) const {
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

        if (hit.material->scatter(ray, hit, &attenuation, &scattered, seed)) {
            return attenuation * computeColor(scattered, scene, depth - 1, seed);
        }

        return { 0.0f, 0.0f, 0.0f };
    }

    const Vec3 unit_direction = normalize(ray.dir);
    const float a = 0.5f * (unit_direction.y + 1.0f);
    return (1.0f - a)* Vec3{ 1.0f, 1.0f, 1.0f } + a * Vec3{ 0.5f, 0.7f, 1.0f };
}

Ray Camera::generateRay(int x, int y, uint32_t* seed) const {
    const Vec3 pixelCenter = m_PixelTopLeft + (x * m_PixelDeltaX) + (y * m_PixelDeltaY);
    const Vec3 pixelSample = pixelCenter + pixelSampleSquare(seed);

    const Vec3 rayOrigin = (defocusAngle > 0.0f) ? defocusDiskSample(seed) : position;
    const Vec3 rayDir = pixelSample - rayOrigin;

    return { rayOrigin, rayDir };
}

Vec3 Camera::pixelSampleSquare(uint32_t* seed) const {
    const float x = -0.5f + randomFloat(seed);
    const float y = -0.5f + randomFloat(seed);

    return x * m_PixelDeltaX + y * m_PixelDeltaY;
}

Vec3 Camera::defocusDiskSample(uint32_t* seed) const {
    const Vec3 p = randomVec3InUnitDisk(seed);

    return position + (p.x * m_DefocusDiskX) + (p.y * m_DefocusDiskY);
}

