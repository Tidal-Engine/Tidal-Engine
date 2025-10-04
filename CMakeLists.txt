cmake_minimum_required(VERSION 3.20)

project(TidalEngine)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option to enable Vulkan validation layers
option(ENABLE_VALIDATION_LAYERS "Enable Vulkan validation layers for debugging" ON)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Add dependencies using FetchContent
include(FetchContent)

# SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.x
    GIT_SHALLOW TRUE
)

# cpptrace
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

# EnTT
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.x
    GIT_SHALLOW TRUE
)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
    GIT_SHALLOW TRUE
)

# ENet
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3 spdlog cpptrace EnTT glm imgui enet)

# Create ImGui library target
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    SDL3::SDL3
    Vulkan::Vulkan
)

# ============================================================================
# Shared Library (used by both client and server)
# ============================================================================
add_library(TidalShared STATIC
    src/shared/Chunk.cpp
    src/shared/ChunkSerializer.cpp
    src/core/ResourceManager.cpp
    src/core/PerformanceMetrics.cpp
)

target_include_directories(TidalShared PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TidalShared PUBLIC
    spdlog::spdlog
    EnTT::EnTT
    glm::glm
)

# ============================================================================
# Dedicated Server (headless, no graphics)
# ============================================================================
add_executable(TidalServer
    src/server/ServerMain.cpp
    src/server/GameServer.cpp
    src/server/World.cpp
)

target_include_directories(TidalServer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TidalServer PRIVATE
    TidalShared
    enet
    spdlog::spdlog
    cpptrace::cpptrace
    EnTT::EnTT
)

# Windows requires ws2_32 and winmm for networking
if(WIN32)
    target_link_libraries(TidalServer PRIVATE ws2_32 winmm)
endif()

target_include_directories(TidalServer PRIVATE
    ${enet_SOURCE_DIR}/include
)

# ============================================================================
# Client (game executable with rendering)
# ============================================================================
add_executable(TidalClient
    src/client/ClientMain.cpp
    src/client/VulkanEngine.cpp
    src/client/VulkanRenderer.cpp
    src/client/InputManager.cpp
    src/client/Camera.cpp
    src/client/NetworkClient.cpp
    src/client/ChunkMesh.cpp
    src/client/ChunkRenderer.cpp
    src/client/TextureAtlas.cpp
    src/client/DebugOverlay.cpp
    src/client/Raycaster.cpp
    src/client/BlockOutlineRenderer.cpp
    src/vulkan/VulkanBuffer.cpp
    src/vulkan/VulkanSwapchain.cpp
    src/vulkan/VulkanPipeline.cpp
    src/vulkan/CubeGeometry.cpp
)

target_include_directories(TidalClient PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(TidalClient PRIVATE
    TidalShared
    SDL3::SDL3
    Vulkan::Vulkan
    spdlog::spdlog
    cpptrace::cpptrace
    EnTT::EnTT
    glm::glm
    imgui
    enet
)

# Windows requires ws2_32 and winmm for networking
if(WIN32)
    target_link_libraries(TidalClient PRIVATE ws2_32 winmm)
endif()

target_include_directories(TidalClient PRIVATE
    ${enet_SOURCE_DIR}/include
)

# Copy SDL3.dll to executable directory on Windows
if(WIN32)
    add_custom_command(TARGET TidalClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:TidalClient>
        COMMENT "Copying SDL3.dll to output directory"
    )
endif()

# Copy resource directories to executable output directory
add_custom_command(TARGET TidalClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:TidalClient>/shaders
    COMMENT "Copying shaders to output directory"
)

add_custom_command(TARGET TidalClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:TidalClient>/assets
    COMMENT "Copying assets to output directory"
)

# Pass validation layer option to client
if(ENABLE_VALIDATION_LAYERS)
    target_compile_definitions(TidalClient PRIVATE ENABLE_VALIDATION_LAYERS)
endif()

# ============================================================================
# Compile shaders
# ============================================================================
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin $ENV{VULKAN_SDK}/Bin)
if(NOT GLSLC)
    message(WARNING "glslc not found - shaders will not be automatically compiled. Please compile shaders manually.")
else()
    set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders)
    set(SHADER_BINARY_DIR ${CMAKE_SOURCE_DIR}/shaders)

    file(GLOB VERTEX_SHADERS ${SHADER_SOURCE_DIR}/*.vert)
    file(GLOB FRAGMENT_SHADERS ${SHADER_SOURCE_DIR}/*.frag)

    foreach(SHADER ${VERTEX_SHADERS})
        get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
        set(SHADER_OUTPUT ${SHADER_BINARY_DIR}/${SHADER_NAME}_vert.spv)
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${GLSLC} ${SHADER} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling vertex shader ${SHADER_NAME}.vert"
        )
        list(APPEND SHADER_BINARIES ${SHADER_OUTPUT})
    endforeach()

    foreach(SHADER ${FRAGMENT_SHADERS})
        get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
        set(SHADER_OUTPUT ${SHADER_BINARY_DIR}/${SHADER_NAME}_frag.spv)
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${GLSLC} ${SHADER} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling fragment shader ${SHADER_NAME}.frag"
        )
        list(APPEND SHADER_BINARIES ${SHADER_OUTPUT})
    endforeach()

    add_custom_target(CompileShaders ALL DEPENDS ${SHADER_BINARIES})
    add_dependencies(TidalClient CompileShaders)
endif()

# ============================================================================
# Convenience targets for running server + client
# ============================================================================

# Helper script to run server then client
if(WIN32)
    set(RUN_SCRIPT_EXT ".bat")
    set(RUN_SCRIPT_CONTENT "@echo off
start \"TidalServer\" /B \"$<TARGET_FILE:TidalServer>\"
set SERVER_PID=%ERRORLEVEL%
\"$<TARGET_FILE:TidalClient>\"
taskkill /F /IM TidalServer.exe >nul 2>&1
")
else()
    set(RUN_SCRIPT_EXT ".sh")
    set(RUN_SCRIPT_CONTENT "#!/bin/bash
\"$<TARGET_FILE:TidalServer>\" &
SERVER_PID=$!
\"$<TARGET_FILE:TidalClient>\"
kill $SERVER_PID 2>/dev/null || true
")
endif()

# Create run script
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/run_game${RUN_SCRIPT_EXT}" CONTENT "${RUN_SCRIPT_CONTENT}")

# Target to run both server and client (uses current build configuration)
add_custom_target(run_game
    COMMAND ${CMAKE_COMMAND} -E echo "Starting TidalServer and TidalClient..."
    COMMAND ${CMAKE_COMMAND} -E chmod +x "${CMAKE_BINARY_DIR}/run_game${RUN_SCRIPT_EXT}"
    COMMAND "${CMAKE_BINARY_DIR}/run_game${RUN_SCRIPT_EXT}"
    DEPENDS TidalServer TidalClient
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running TidalServer (background) and TidalClient (foreground)"
    USES_TERMINAL
)

# Target to build server (Release) + client (Debug), then run both
# Note: This works best with multi-config generators (Visual Studio, Xcode)
add_custom_target(run_game_dev
    COMMAND ${CMAKE_COMMAND} -E echo "Building TidalServer (Release) and TidalClient (Debug)..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target TidalServer --config Release
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target TidalClient --config Debug
    COMMAND ${CMAKE_COMMAND} -E echo "Starting server and client..."
    COMMAND ${CMAKE_COMMAND} -E chmod +x "${CMAKE_BINARY_DIR}/run_game${RUN_SCRIPT_EXT}"
    COMMAND "${CMAKE_BINARY_DIR}/run_game${RUN_SCRIPT_EXT}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Build (Server=Release, Client=Debug) and run both"
    USES_TERMINAL
)

# Target to build server (Release) + client (Release), then run both
add_custom_target(run_game_release
    COMMAND ${CMAKE_COMMAND} -E echo "Building TidalServer and TidalClient (Release)..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target TidalServer --config Release
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target TidalClient --config Release
    COMMAND ${CMAKE_COMMAND} -E echo "Starting server and client..."
    COMMAND ${CMAKE_COMMAND} -E chmod +x "${CMAKE_BINARY_DIR}/run_game${RUN_SCRIPT_EXT}"
    COMMAND "${CMAKE_BINARY_DIR}/run_game${RUN_SCRIPT_EXT}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Build (Server=Release, Client=Release) and run both"
    USES_TERMINAL
)
