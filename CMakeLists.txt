cmake_minimum_required(VERSION 3.20)

# Set generator to Ninja
set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)

# Set compilers BEFORE project()
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

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

FetchContent_MakeAvailable(SDL3 spdlog cpptrace EnTT glm imgui)

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

# Add executable
add_executable(TidalEngine
    src/Main.cpp
    src/core/ResourceManager.cpp
    src/vulkan/VulkanEngine.cpp
    src/vulkan/VulkanBuffer.cpp
    src/vulkan/VulkanSwapchain.cpp
    src/vulkan/VulkanPipeline.cpp
    src/vulkan/VulkanRenderer.cpp
    src/vulkan/CubeGeometry.cpp
)

# Include directories
target_include_directories(TidalEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(TidalEngine
    SDL3::SDL3
    Vulkan::Vulkan
    spdlog::spdlog
    cpptrace::cpptrace
    EnTT::EnTT
    glm::glm
    imgui
)

# Pass validation layer option to code
if(ENABLE_VALIDATION_LAYERS)
    target_compile_definitions(TidalEngine PRIVATE ENABLE_VALIDATION_LAYERS)
endif()