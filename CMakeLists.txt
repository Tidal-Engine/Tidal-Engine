cmake_minimum_required(VERSION 3.25)
project(TidalEngine VERSION 1.0.0 LANGUAGES CXX)

# C++23 Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# Find packages (cross-platform)
if(WIN32)
    # Windows with vcpkg
    find_package(glfw3 REQUIRED)
    find_package(glm REQUIRED)
else()
    # Linux/Unix with pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    find_package(glm REQUIRED)
endif()

# Dear ImGui - fetch from GitHub
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

# Doxygen documentation support
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # Configure Doxyfile with CMake variables
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    # Add documentation target
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)

    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE}")
    message(STATUS "Documentation can be built with 'make doc' or 'cmake --build . --target doc'")
else()
    message(WARNING "Doxygen not found - documentation target not available")
endif()

# Create ImGui library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

if(WIN32)
    target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${Vulkan_INCLUDE_DIRS}
    )
else()
    target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${Vulkan_INCLUDE_DIRS}
        ${GLFW_INCLUDE_DIRS}
    )
endif()

if(WIN32)
    target_link_libraries(imgui_lib
        ${Vulkan_LIBRARIES}
        glfw
    )
else()
    target_link_libraries(imgui_lib
        ${Vulkan_LIBRARIES}
        ${GLFW_LIBRARIES}
    )
endif()

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
    src/Main.cpp
    src/vulkan/VulkanDevice.cpp
    src/vulkan/VulkanRenderer.cpp
    src/vulkan/VulkanBuffer.cpp
    src/core/Camera.cpp
    src/game/Chunk.cpp
    src/game/ChunkManager.cpp
    src/graphics/TextureManager.cpp
    src/system/UserDataManager.cpp
    src/game/SaveSystem.cpp
    src/game/GameServer.cpp
    src/network/NetworkProtocol.cpp
    src/network/NetworkManager.cpp
    src/core/ThreadPool.cpp
    src/game/GameClient.cpp
    src/game/GameClientRenderer.cpp
    src/game/GameClientInput.cpp
    src/game/GameClientNetwork.cpp
    src/game/GameClientUI.cpp
    src/game/ClientChunkManager.cpp
    src/game/ClientChunk.cpp
)

# Header files
set(HEADERS
    include/vulkan/VulkanDevice.h
    include/vulkan/VulkanRenderer.h
    include/vulkan/VulkanBuffer.h
    include/core/Camera.h
    include/game/Chunk.h
    include/game/ChunkManager.h
    include/graphics/TextureManager.h
    include/system/UserDataManager.h
    include/game/SaveSystem.h
    include/game/GameServer.h
    include/network/NetworkProtocol.h
    include/network/NetworkManager.h
    include/core/ThreadPool.h
    include/game/GameClient.h
    include/game/GameClientRenderer.h
    include/game/GameClientInput.h
    include/game/GameClientNetwork.h
    include/game/GameClientUI.h
    include/game/ClientChunkManager.h
    include/game/ClientChunk.h
)

# Create client executable
add_executable(TidalEngine ${SOURCES} ${HEADERS})

# Create server-only sources (no graphics dependencies)
set(SERVER_SOURCES
    src/ServerMain.cpp
    src/game/SaveSystem.cpp
    src/game/GameServer.cpp
    src/network/NetworkProtocol.cpp
    src/core/ThreadPool.cpp
    src/network/NetworkManager.cpp
)

set(SERVER_HEADERS
    include/game/SaveSystem.h
    include/game/GameServer.h
    include/network/NetworkProtocol.h
    include/core/ThreadPool.h
    include/network/NetworkManager.h
)

# Create headless server executable
add_executable(TidalServer ${SERVER_SOURCES} ${SERVER_HEADERS})

# Include directories
if(WIN32)
    target_include_directories(TidalEngine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
else()
    target_include_directories(TidalEngine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS}
        ${GLFW_INCLUDE_DIRS}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        /usr/include/stb
    )
endif()

# Link libraries
if(WIN32)
    target_link_libraries(TidalEngine
        ${Vulkan_LIBRARIES}
        glfw
        glm::glm
        imgui_lib
    )
else()
    target_link_libraries(TidalEngine
        ${Vulkan_LIBRARIES}
        ${GLFW_LIBRARIES}
        glm::glm
        imgui_lib
    )
endif()

# Configure server executable (headless, no graphics)
if(WIN32)
    target_include_directories(TidalServer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
else()
    target_include_directories(TidalServer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        /usr/include/stb
    )
endif()

target_compile_definitions(TidalServer PRIVATE HEADLESS_SERVER)

target_link_libraries(TidalServer
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
    target_compile_options(TidalServer PRIVATE /W4)
else()
    target_compile_options(TidalEngine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(TidalServer PRIVATE -Wall -Wextra -Wpedantic)
endif()