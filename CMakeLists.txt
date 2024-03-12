cmake_minimum_required(VERSION 3.1.0)

project(stingray LANGUAGES CXX)

# Settings
set(DCMAKE_GENERATOR_PLATFORM "x64")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /fp:fast")
else()
    set(CMAKE_BUILD_TYPE Release)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math")
endif()

set(SOURCE_FILES
    src/camera.cpp
    src/camera.h
    src/hittable.h
    src/main.cpp
    src/material.cpp
    src/material.h
    src/scene.cpp
    src/scene.h
    src/sphere.cpp
    src/sphere.h

    src/math/sray_math.h
    src/utility/perfTimer.cpp
    src/utility/perfTimer.h
    src/vendor/stb_image_write.h
)

add_executable(stingray ${SOURCE_FILES})
