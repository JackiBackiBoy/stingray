cmake_minimum_required(VERSION 3.1.0)

project(stingray-gui LANGUAGES CXX)

# Dependency paths
set(FREETYPE_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/vendor/freetype/include/)
set(DXSDK_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/vendor/dxsdk/include/)
set(DXSDK_BIN_DIR ${CMAKE_HOME_DIRECTORY}/vendor/dxsdk/bin/)
set(DXC_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/vendor/dxc/include/)
set(DXCOMPILER_DLL_PATH ${CMAKE_HOME_DIRECTORY}/vendor/dxc/binaries/dxcompiler.dll)
set(DXIL_DLL_PATH ${CMAKE_HOME_DIRECTORY}/vendor/dxc/binaries/dxil.dll)
set(GLM_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/vendor/glm/include/)

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
    ${SOURCE_DIR}/core/enum_flags.hpp
    ${SOURCE_DIR}/core/frame_info.hpp
    ${SOURCE_DIR}/core/settings.hpp
    ${SOURCE_DIR}/core/utilities.cpp
    ${SOURCE_DIR}/core/utilities.hpp
    ${SOURCE_DIR}/core/window.hpp
    ${SOURCE_DIR}/core/window_base.hpp

    # Data
    ${SOURCE_DIR}/data/entity.hpp
    ${SOURCE_DIR}/data/font.cpp
    ${SOURCE_DIR}/data/font.hpp
    ${SOURCE_DIR}/data/model.hpp
    ${SOURCE_DIR}/data/scene.cpp
    ${SOURCE_DIR}/data/scene.hpp
    
    # Input
    ${SOURCE_DIR}/input/keycode.hpp
    ${SOURCE_DIR}/input/input.hpp
    ${SOURCE_DIR}/input/mousebutton.hpp

    # Math
    ${SOURCE_DIR}/math/quat.hpp
    ${SOURCE_DIR}/math/sr_math.hpp

    # Managers
    ${SOURCE_DIR}/managers/asset_manager.cpp
    ${SOURCE_DIR}/managers/asset_manager.hpp

    # Rendering
    ${SOURCE_DIR}/rendering/device.hpp
    ${SOURCE_DIR}/rendering/graphics.hpp
    ${SOURCE_DIR}/rendering/render_graph.cpp
    ${SOURCE_DIR}/rendering/render_graph.hpp

    # Renderpasses
    ${SOURCE_DIR}/rendering/renderpasses/accumulation_pass.cpp
    ${SOURCE_DIR}/rendering/renderpasses/accumulation_pass.hpp
    ${SOURCE_DIR}/rendering/renderpasses/fullscreen_tri_pass.cpp
    ${SOURCE_DIR}/rendering/renderpasses/fullscreen_tri_pass.hpp
    ${SOURCE_DIR}/rendering/renderpasses/gbuffer_pass.cpp
    ${SOURCE_DIR}/rendering/renderpasses/gbuffer_pass.hpp
    ${SOURCE_DIR}/rendering/renderpasses/rtao_pass.cpp
    ${SOURCE_DIR}/rendering/renderpasses/rtao_pass.hpp
    ${SOURCE_DIR}/rendering/renderpasses/simple_shadow_pass.cpp
    ${SOURCE_DIR}/rendering/renderpasses/simple_shadow_pass.hpp
    ${SOURCE_DIR}/rendering/renderpasses/ui_pass.cpp
    ${SOURCE_DIR}/rendering/renderpasses/ui_pass.hpp

    # UI
    ${SOURCE_DIR}/ui/ui_event.cpp
    ${SOURCE_DIR}/ui/ui_event.hpp

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

        # Input
        ${SOURCE_DIR}/input/windows/input_win32.cpp

        # DirectX 12
        ${SOURCE_DIR}/rendering/dx12/device_dx12.cpp
        ${SOURCE_DIR}/rendering/dx12/device_dx12.hpp
        ${SOURCE_DIR}/rendering/dx12/graphics_dx12.hpp
    )
elseif(APPLE)

endif()

# Platform-specific options
if (WIN32)
    add_executable(stingray-gui WIN32 ${SOURCE_FILES} ${SOURCE_FILES_PLATFORM})

    target_compile_definitions(stingray-gui PRIVATE
        SR_WINDOWS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        NOGDICAPMASKS
        NOCRYPT
        NOMENUS
        NODRAWTEXT
        NOMEMMGR
        NOCOMM
        NOKANJI

        UNICODE
        _UNICODE
        GLM_FORCE_LEFT_HANDED
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
    )

    target_include_directories(stingray-gui PRIVATE
        ${DXSDK_INCLUDE_DIR}
        ${DXC_INCLUDE_DIR}
        ${GLM_INCLUDE_DIR}
        ${FREETYPE_INCLUDE_DIR}
    )

    target_link_libraries(stingray-gui PUBLIC
        D3D12
        DXGI
        freetype
    )
elseif(APPLE)

endif()

# ------ Custom build commands ------
if (WIN32)
    # Copy DX12 Dependencies
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${DXSDK_BIN_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/D3D12"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DXCOMPILER_DLL_PATH}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/dxcompiler.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DXIL_DLL_PATH}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/dxil.dll"
        COMMENT "Copying DirectX 12 Agility SDK, dxcompiler.dll and dxil.dll to runtime output directory"
    )

    add_custom_target(copy_dependencies_DX12 ALL DEPENDS ${PROJECT_NAME}
        COMMENT "Ensuring DirectX 12 Agility SDK, dxcompiler.dll and dxil.dll are copied to runtime output directory"
    )
endif()

