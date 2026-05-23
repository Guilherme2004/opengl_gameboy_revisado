cmake_minimum_required(VERSION 3.16)
project(GameBoyParallaxOpenGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG QUIET)

add_executable(gameboy_parallax
    src/main.cpp
)

if (TARGET glfw)
    set(GLFW_TARGET glfw)
elseif (TARGET glfw3)
    set(GLFW_TARGET glfw3)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
    set(GLFW_TARGET PkgConfig::GLFW3)
endif()

target_link_libraries(gameboy_parallax
    PRIVATE
    OpenGL::GL
    GLEW::GLEW
    ${GLFW_TARGET}
)

if (MSVC)
    target_compile_options(gameboy_parallax PRIVATE /W4)
else()
    target_compile_options(gameboy_parallax PRIVATE -Wall -Wextra -pedantic)
endif()
