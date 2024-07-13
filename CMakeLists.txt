cmake_minimum_required(VERSION 3.1.0)

project(stingray LANGUAGES CXX)

# Settings
set(DCMAKE_GENERATOR_PLATFORM "x64")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options

add_subdirectory(stingray-cli)
add_subdirectory(stingray-gui)