#!/bin/bash
clang++ -std=c++17 -O3 src/main.cpp src/camera.cpp src/material.cpp src/scene.cpp src/sphere.cpp -o cli
./cli
