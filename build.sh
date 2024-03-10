#!/bin/bash
clang++ -std=c++17 -Ofast src/main.cpp src/camera.cpp src/material.cpp src/scene.cpp src/sphere.cpp src/utility/perfTimer.cpp -o cli
./cli
