cmake_minimum_required(VERSION 3.9)
project(bigged)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(bigg/deps)
include(bigg/deps/bgfx.cmake/cmake/util/ConfigureDebugging.cmake)

link_libraries(
        bgfx
        imgui
        glfw
        glm
        )

include_directories(bigged
        PUBLIC
        bigg/include
        entt/src/entt
        )

add_library(bigged
        bigg/src/bigg.cpp
        bigg/deps/bgfx.cmake/bgfx/examples/common/bounds.cpp
        bigg/deps/bgfx.cmake/bgfx/examples/common/debugdraw/debugdraw.cpp
        bigged.cpp
        )