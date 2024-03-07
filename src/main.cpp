#include <cstdint>
#include <limits>
#include <iostream>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"

#include "camera.h"
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

    m_Camera = std::make_unique<Camera>(width, height);

    m_Scene = std::make_unique<Scene>();
    m_Scene->add(std::make_shared<Sphere>(Vec3{ 0.0f, 0.0f, -1.0f }, 0.5f));
    m_Scene->add(std::make_shared<Sphere>(Vec3{ 0.0f, -100.5f, -1.0f }, 100.0f));

    m_Camera->render(*m_Scene, pixels.data());

    stbi_write_png("image.png", width, height, 4, pixels.data(), width * 4);

    return 0;
}
