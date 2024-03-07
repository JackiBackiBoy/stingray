#include <cstdint>
#include <limits>
#include <iostream>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"

#include "math/sray_math.h"
#include "utility/perfTimer.h"
#include "scene.h"
#include "sphere.h"

static Vec3 cameraPos = { 0.0f, 0.0f, -2.0f };
static int WIDTH = 0;
static int HEIGHT = 0;
static float ASPECT_RATIO = 1.0f;
static std::unique_ptr<Scene> m_Scene{};

inline uint32_t perPixel(Ray ray) {
    uint32_t pixelColor = 0xff000000; // black
    const Vec3 lightDir = normalize({ 0.3f, 0.0f, -1.0f });
    HitData hit{};

    if (m_Scene->hit(ray, 0, std::numeric_limits<float>::infinity(), &hit)) {
        return rgbToHex(0.5f * (hit.normal + Vec3{1, 1, 1}));
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
            coord.y = -coord.y;
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

    m_Scene = std::make_unique<Scene>();

    m_Scene->add(std::make_shared<Sphere>(
        Vec3{ -0.3f, 0.8f, 0.0f },
        0.5f
    ));

    //m_Spheres.push_back(sphere1);
    //m_Spheres.push_back(sphere2);

    render(pixels.data());
    stbi_write_png("image.png", WIDTH, HEIGHT, 4, pixels.data(), WIDTH * 4);

    return 0;
}
