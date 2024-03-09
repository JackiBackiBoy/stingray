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

static std::unique_ptr<Camera> m_Camera{};
static std::unique_ptr<Scene> m_Scene{};

int main(int argc, char* argv[]) {
    int width = 0;
    int height = 0;

    std::cout << "Image Width: ";
    std::cin >> width;
    std::cout << "Image Height: ";
    std::cin >> height;

    std::vector<uint32_t> pixels(width * height, 0xff000000); // black background

    // Materials
    DiffuseMaterial materialGround({ 0.8f, 0.8f, 0.0f });
    DiffuseMaterial materialCenter({ 0.1f, 0.2f, 0.5f });
    DielectricMaterial materialLeft(1.5f);
    MetalMaterial materialRight({ 0.8f, 0.6f, 0.2f }, 0.0f);

    // Scene
    Sphere ground(Vec3{ 0.0f, -100.5f, -1.0f }, 100.0f, &materialGround);
    Sphere center(Vec3{ 0.0f, 0.0f, -1.0f }, 0.5f, &materialCenter);
    Sphere left(Vec3{ -1.0f, 0.0f, -1.0f }, 0.5f, &materialLeft);
    Sphere leftBubble(Vec3{ -1.0f, 0.0f, -1.0f }, -0.4f, &materialLeft);
    Sphere right(Vec3{ 1.0f, 0.0f, -1.0f }, 0.5f, &materialRight);

    m_Scene = std::make_unique<Scene>();
    m_Scene->add(&ground);
    m_Scene->add(&center);
    m_Scene->add(&left);
    m_Scene->add(&leftBubble);
    m_Scene->add(&right);

    m_Camera = std::make_unique<Camera>(width, height);
    m_Camera->maxDepth = 50;
    m_Camera->position = { -2.0f, 2.0f, 1.0f };
    m_Camera->lookAt = { 0.0f, 0.0f, -1.0f };
    m_Camera->up = { 0.0f, 1.0f, 0.0f };
    m_Camera->verticalFOV = Pi * 0.2f;

    m_Camera->render(*m_Scene, pixels.data());

    stbi_write_png("image.png", width, height, 4, pixels.data(), width * 4);

    return 0;
}
