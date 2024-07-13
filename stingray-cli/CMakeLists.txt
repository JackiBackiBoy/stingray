cmake_minimum_required(VERSION 3.1.0)

project(stingray-cli LANGUAGES CXX)

# Settings
set(DCMAKE_GENERATOR_PLATFORM "x64")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /fp:fast")
    set(INT_DIR ${CMAKE_HOME_DIRECTORY}/x64/stingray-cli)

    # Build output directory
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${INT_DIR}/${OUTPUTCONFIG})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${INT_DIR}/${OUTPUTCONFIG})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${INT_DIR}/${OUTPUTCONFIG})
    endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)
else()
    set(CMAKE_BUILD_TYPE Release)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math")
endif()

# Source files
set(SOURCE_DIR ${CMAKE_HOME_DIRECTORY}/stingray-cli/src)

set(SOURCE_FILES
    ${SOURCE_DIR}/camera.cpp
    ${SOURCE_DIR}/camera.h
    ${SOURCE_DIR}/hittable.h
    ${SOURCE_DIR}/main.cpp
    ${SOURCE_DIR}/material.cpp
    ${SOURCE_DIR}/material.h
    ${SOURCE_DIR}/scene.cpp
    ${SOURCE_DIR}/scene.h
    ${SOURCE_DIR}/sphere.cpp
    ${SOURCE_DIR}/sphere.h

    ${SOURCE_DIR}/math/sray_math.h
    ${SOURCE_DIR}/utility/perfTimer.cpp
    ${SOURCE_DIR}/utility/perfTimer.h
    ${SOURCE_DIR}/vendor/stb_image_write.h
)

add_executable(stingray-cli ${SOURCE_FILES})
