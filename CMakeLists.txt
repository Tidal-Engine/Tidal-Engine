cmake_minimum_required(VERSION 3.25)
project(TidalEngine VERSION 1.0.0 LANGUAGES CXX)

# C++23 Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# System package integration
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
find_package(glm REQUIRED)

# Vulkan validation layers setup for debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(ENABLE_VALIDATION_LAYERS)
endif()

# SPIR-V shader compilation
find_program(GLSL_VALIDATOR glslangValidator HINTS ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE})

function(compile_shader target shader_file)
    get_filename_component(file_name ${shader_file} NAME)
    set(spirv_file "${CMAKE_CURRENT_BINARY_DIR}/shaders/compiled/${file_name}.spv")

    add_custom_command(
        OUTPUT ${spirv_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders/compiled/"
        COMMAND ${GLSL_VALIDATOR} -V ${shader_file} -o ${spirv_file}
        DEPENDS ${shader_file}
        COMMENT "Compiling ${file_name} to SPIR-V")

    target_sources(${target} PRIVATE ${spirv_file})
endfunction()

# Source files
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/VulkanDevice.cpp
    src/VulkanRenderer.cpp
    src/VulkanBuffer.cpp
    src/Camera.cpp
)

# Header files
set(HEADERS
    include/Application.h
    include/VulkanDevice.h
    include/VulkanRenderer.h
    include/VulkanBuffer.h
    include/Camera.h
)

# Create executable
add_executable(TidalEngine ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(TidalEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    /usr/include/stb
)

# Link libraries
target_link_libraries(TidalEngine
    ${Vulkan_LIBRARIES}
    ${GLFW_LIBRARIES}
    glm::glm
)

# Compile shaders
compile_shader(TidalEngine "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vertex.vert")
compile_shader(TidalEngine "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fragment.frag")

# Copy assets to build directory
add_custom_command(TARGET TidalEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets
)

# Copy compiled shaders to runtime directory
add_custom_command(TARGET TidalEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_BINARY_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(TidalEngine PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(TidalEngine PRIVATE VK_USE_PLATFORM_XLIB_KHR)
elseif(APPLE)
    target_compile_definitions(TidalEngine PRIVATE VK_USE_PLATFORM_METAL_EXT)
endif()

# Compiler-specific flags
if(MSVC)
    target_compile_options(TidalEngine PRIVATE /W4)
else()
    target_compile_options(TidalEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()