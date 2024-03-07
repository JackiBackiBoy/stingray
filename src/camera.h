#pragma once

#include "scene.h"

class Camera {
public:
    Camera() {};
    ~Camera() {};

    int imageWidth = 128;
    int imageHeight = 128;
    int samplesPerPixel = 1;

    void render(const Scene& scene);

private:
};
