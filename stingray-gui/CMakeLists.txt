cmake_minimum_required(VERSION 3.1.0)

project(stingray-gui LANGUAGES CXX)

# Settings
set(DCMAKE_GENERATOR_PLATFORM "x64")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /fp:fast")
    set(INT_DIR ${CMAKE_HOME_DIRECTORY}/x64/stingray-gui)

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
set(SOURCE_DIR ${CMAKE_HOME_DIRECTORY}/stingray-gui/src)

set(SOURCE_FILES
    ${SOURCE_DIR}/main.cpp

    # Core
    ${SOURCE_DIR}/core/application.cpp
    ${SOURCE_DIR}/core/application.hpp
    ${SOURCE_DIR}/core/utilities.cpp
    ${SOURCE_DIR}/core/utilities.hpp
    ${SOURCE_DIR}/core/window.hpp
    ${SOURCE_DIR}/core/window_base.hpp

    # Data
    ${SOURCE_DIR}/data/entity.hpp
    ${SOURCE_DIR}/data/model.hpp
    
    # Input
    ${SOURCE_DIR}/input/keycode.hpp
    ${SOURCE_DIR}/input/rawinput.cpp
    ${SOURCE_DIR}/input/rawinput.hpp

    # Math
    ${SOURCE_DIR}/math/quat.hpp

    # Managers
    ${SOURCE_DIR}/managers/asset_manager.cpp
    ${SOURCE_DIR}/managers/asset_manager.hpp

    # Rendering
    ${SOURCE_DIR}/rendering/device.hpp
    ${SOURCE_DIR}/rendering/graphics.hpp

    # Utility
    ${SOURCE_DIR}/utility/json.hpp
    ${SOURCE_DIR}/utility/stb_image.h
    ${SOURCE_DIR}/utility/stb_image_write.h
    ${SOURCE_DIR}/utility/tiny_gltf.h
    ${SOURCE_DIR}/utility/utility.cpp
)

# Platform-specific source files
if (WIN32)
    set(SOURCE_FILES_PLATFORM
        ${SOURCE_DIR}/windows/window_win32.cpp
        ${SOURCE_DIR}/windows/window_win32.hpp

        # DirectX 12
        ${SOURCE_DIR}/rendering/dx12/d3dx12.h
        ${SOURCE_DIR}/rendering/dx12/device_dx12.cpp
        ${SOURCE_DIR}/rendering/dx12/device_dx12.hpp
        ${SOURCE_DIR}/rendering/dx12/graphics_dx12.hpp
    )
elseif(APPLE)

endif()

add_executable(stingray-gui ${SOURCE_FILES} ${SOURCE_FILES_PLATFORM})

# Platform-specific options
if (WIN32)
    set(DXC_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/vendor/dxc/include/)
    set(GLM_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/vendor/glm/include/)

    target_compile_definitions(stingray-gui PRIVATE
        SR_WINDOWS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        GLM_FORCE_LEFT_HANDED
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
    )

    target_include_directories(stingray-gui PRIVATE
        ${DXC_INCLUDE_DIR}
        ${GLM_INCLUDE_DIR}
    )

    target_link_libraries(stingray-gui PUBLIC
        D3D12
        DXGI
    )
elseif(APPLE)

endif()


# Target libraries

