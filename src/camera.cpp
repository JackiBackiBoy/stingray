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

    // Note: Given N threads, we will assign chunks of rows of the
    // image buffer to each thread. Ideally, each one of these
    // chunks should have the same size, though that is dependent
    // on the number of threads in use and the image height
    const int baseRowsPerThread = imageHeight / numThreads;

    std::vector<int> rowsPerThread(numThreads, baseRowsPerThread);
    std::vector<std::thread> threads(numThreads);

    int extraRows = imageHeight % numThreads;
    size_t index = 0;

    while (extraRows > 0) {
        ++rowsPerThread[index];
        ++index;
        --extraRows;
    }

    int yStart = 0;
    for (size_t i = 0; i < numThreads; ++i) {
        threads[i] = std::thread(
            renderChunk,
            imageBuffer,
            yStart,
            rowsPerThread[i],
            imageWidth,
            scene,
            this);

        yStart += rowsPerThread[i];
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
    int yStart,
    int numRows,
    int imageWidth,
    const Scene& scene,
    Camera* camera) {

    uint32_t seed = std::hash<std::thread::id>()(std::this_thread::get_id());
    seed += 1;

    for (int y = yStart; y < yStart + numRows; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            Vec3 pixelColor = { 0.0f, 0.0f, 0.0f };

            for (int sample = 0; sample < camera->samplesPerPixel; ++sample) {
                const Ray ray = camera->generateRay(x, y, &seed);
                pixelColor += camera->computeColor(ray, scene, camera->maxDepth, &seed);
            }

            const float scale = 1.0f / camera->samplesPerPixel;
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
    const float a = 0.5 * (unit_direction.y + 1.0);
    return (1.0 - a)* Vec3{1.0, 1.0, 1.0} + a * Vec3{0.5, 0.7, 1.0};
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

