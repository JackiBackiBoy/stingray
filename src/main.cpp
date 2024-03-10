#include <cstdint>
#include <limits>
#include <iostream>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"

#include "camera.h"
#include "material.h"
#include "scene.h"
#include "sphere.h"
#include "math/sray_math.h"
#include "utility/perfTimer.h"

int main(int argc, char* argv[]) {
    int width = 0;
    int height = 0;

    std::cout << "Image Width: ";
    std::cin >> width;
    std::cout << "Image Height: ";
    std::cin >> height;

    std::vector<uint32_t> pixels(width * height, 0xff000000); // black background

    Scene m_Scene{};
    Camera m_Camera(width, height);

    // Materials
    DiffuseMaterial materialGround({ 0.3f, 0.3f, 0.3f });
    DiffuseMaterial materialCenter({ 0.4f, 0.2f, 0.1f });
    DielectricMaterial materialLeft(1.5f);
    MetalMaterial materialRight({ 0.7f, 0.6f, 0.5f }, 0.0f);

    // Scene
    Sphere left(Vec3{ 0.0f, 1.0f, 0.0f }, 1.0f, &materialLeft);
    Sphere center(Vec3{ -4.0f, 1.0f, 0.0f }, 1.0f, &materialCenter);
    Sphere right(Vec3{ 4.0f, 1.0f, 0.0f }, 1.0f, &materialRight);
    Sphere ground(Vec3{ 0.0f, -1000.0f, 0.0f }, 1000.0f, &materialGround);

    m_Scene.add(&ground);
    m_Scene.add(&center);
    m_Scene.add(&left);
    m_Scene.add(&right);

    // Randomize spheres
    std::vector<DiffuseMaterial> sphereMaterials{};
    sphereMaterials.reserve(100);
    std::vector<Sphere> spheres{};
    spheres.reserve(100);

    for (size_t i = 0; i < 100; ++i) {
        DiffuseMaterial material({ randomFloat(), randomFloat(), randomFloat() });
        sphereMaterials.push_back(material);

        const Vec3 p = { randomFloat(-7.0f, 7.0f), 0.2f, randomFloat(-7.0f, 7.0f) };

        Sphere sphere(p, 0.2f, &sphereMaterials[i]);
        spheres.push_back(sphere);

        m_Scene.add(&spheres[i]);
    }

    m_Camera.maxDepth = 50;
    m_Camera.position = { 13.0f, 2.0f, 3.0f };
    m_Camera.lookAt = { 0.0f, 0.0f, 0.0f };
    m_Camera.up = { 0.0f, 1.0f, 0.0f };
    m_Camera.verticalFOV = toRadians(20.0f);
    m_Camera.defocusAngle = toRadians(0.0f);
    m_Camera.focusDistance = 10.0f;
    m_Camera.samplesPerPixel = 100;

    PerfTimer timer{};
    timer.begin();
    m_Camera.render(m_Scene, pixels.data());
    timer.end();

    std::cout << timer.getElapsedTime() << '\n';

    stbi_write_png("image.png", width, height, 4, pixels.data(), width * 4);

    return 0;
}
