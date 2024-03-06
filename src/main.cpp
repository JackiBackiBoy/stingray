#include <cstdint>
#include <iostream>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"

#include "math/sray_math.h"

static vec3 cameraPos = { -2.0f, 0.0f, 0.0f };
static int WIDTH = 0;
static int HEIGHT = 0;
static float ASPECT_RATIO = 1.0f;

struct Material {
    vec3 color{}; // range 0-1 per channel (RGB)
};

struct Sphere {
    vec3 position{};
    float radius = 1.0f;
    Material material{};
};

inline uint32_t rgbToHex(vec3 color) {
    uint8_t r = (uint8_t)(color.x * 255.0f);
    uint8_t g = (uint8_t)(color.y * 255.0f);
    uint8_t b = (uint8_t)(color.z * 255.0f);

    return 0xff000000 | (b << 16) | (g << 8) | r;
}

uint32_t perPixel(vec2 coord) {
    vec3 rayOrigin = { 0.0f, 0.0f, -2.0f };
    vec3 rayDir = { coord.x, coord.y, 1.0f };
    float radius = 0.5f;

    float a = dot(rayDir, rayDir);
    float b = dot(rayOrigin, rayDir);
    float c = dot(rayOrigin, rayOrigin) - radius * radius;

    float discriminant = b * b - a * c;

    if (discriminant < 0.0f) { // sphere was not hit
        return 0xff000000;
    }

    float denomQuotient = 1.0f / a;
    float sqrtTerm = sqrtf(discriminant);
    float t1 = (-b + sqrtTerm) * denomQuotient;
    float t2 = (-b - sqrtTerm) * denomQuotient;
    float tMin = abs(t1) < abs(t2) ? t1 : t2;

    vec3 closestHitPoint = rayOrigin + rayDir * tMin;
    vec3 normal = closestHitPoint; // only works for origin (0, 0, 0)
    normal = normalize(normal);

    vec3 lightDir = { 0.3f, 0.0f, -1.0f };
    lightDir = normalize(lightDir);

    float intensity = std::max(dot(normal, lightDir), 0.0f);
    uint8_t color = (uint8_t)(255.0f * intensity);
    return 0xff000000 | color;
}

void render(uint32_t* const buffer) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            vec2 coord = { (float)x / WIDTH, (float)y / HEIGHT };
            coord = (coord * 2.0f) - vec2{1.0f, 1.0f};
            coord.x *= ASPECT_RATIO;

            buffer[y * WIDTH + x] = perPixel(coord);
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

    Sphere sphere{}; // default sphere
    sphere.material.color = { 1.0f, 1.0f, 0.0f };

    render(pixels.data());

    stbi_write_png("image.png", WIDTH, HEIGHT, 4, pixels.data(), WIDTH * 4);

    return 0;
}
