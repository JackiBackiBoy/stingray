#include <cstdint>
#include <iostream>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"

#include "math/sray_math.h"
#include "utility/perfTimer.h"

struct Material {
    Vec3 color{}; // range 0-1 per channel (RGB)
};

struct Sphere {
    Vec3 position{};
    float radius = 1.0f;
    Material material{};
};

static Vec3 cameraPos = { 0.0f, 0.0f, -2.0f };
static int WIDTH = 0;
static int HEIGHT = 0;
static float ASPECT_RATIO = 1.0f;
static std::vector<Sphere> m_Spheres{};

inline uint32_t rgbToHex(Vec3 color) {
    const uint8_t r = (uint8_t)(color.x * 255.0f);
    const uint8_t g = (uint8_t)(color.y * 255.0f);
    const uint8_t b = (uint8_t)(color.z * 255.0f);

    return 0xff000000 | (b << 16) | (g << 8) | r;
}

inline uint32_t perPixel(Ray ray) {
    uint32_t pixelColor = 0xff000000; // black

    for (size_t i = 0; i < m_Spheres.size(); ++i) {
        const Sphere& sphere = m_Spheres[i];

        const Vec3 oc = ray.origin - sphere.position;
        const float a = dot(ray.dir, ray.dir);
        const float bHalf = dot(ray.dir, oc);
        const float c = dot(oc, oc) - sphere.radius * sphere.radius;
        const float discriminant = bHalf * bHalf - a * c;

        if (discriminant < 0.0f) { // sphere was not hit
            continue;
        }

        const float invDenom = 1.0f / a;
        const float sqrtTerm = sqrtf(discriminant);
        const float t1 = (-bHalf + sqrtTerm) * invDenom;
        const float t2 = (-bHalf - sqrtTerm) * invDenom;
        const float tMin = abs(t1) < abs(t2) ? t1 : t2;

        const Vec3 closestHitPoint = ray.at(tMin);
        const Vec3 normal = normalize(closestHitPoint - sphere.position);
        const Vec3 lightDir = normalize({ 0.3f, 0.0f, -1.0f });
        const float intensity = std::max(dot(normal, lightDir), 0.0f);

        pixelColor = rgbToHex(sphere.material.color * intensity);
    }

    return pixelColor;
}

void render(uint32_t* const buffer) {
    Ray ray{};
    ray.origin = cameraPos;

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            Vec2 coord = { (float)x / WIDTH, (float)y / HEIGHT };
            coord = (coord * 2.0f) - Vec2{1.0f, 1.0f};
            coord.x *= ASPECT_RATIO;

            ray.dir = { coord.x, coord.y, 1.0f };
            buffer[y * WIDTH + x] = perPixel(ray);
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Stingray CLI\n";
    std::cout << "Image Width: ";
    std::cin >> WIDTH;
    std::cout << "Image Height: ";
    std::cin >> HEIGHT;

    ASPECT_RATIO = (float)WIDTH / HEIGHT;
    std::vector<uint32_t> pixels(WIDTH * HEIGHT, 0xff000000); // black background

    Sphere sphere1{};
    sphere1.position = { -0.3f, 0.0f, 0.0f };
    sphere1.radius = 0.5f;
    sphere1.material.color = { 0.6f, 0.3f, 0.9f };

    Sphere sphere2{};
    sphere2.position = { 2.0f, 0.2f, 0.7f };
    sphere2.radius = 0.5f;
    sphere2.material.color = { 0.0f, 1.0f, 0.0f };

    m_Spheres.push_back(sphere1);
    m_Spheres.push_back(sphere2);

    render(pixels.data());
    stbi_write_png("image.png", WIDTH, HEIGHT, 4, pixels.data(), WIDTH * 4);

    return 0;
}
