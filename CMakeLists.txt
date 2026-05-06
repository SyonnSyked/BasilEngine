cmake_minimum_required(VERSION 3.20)

project(BasilEngine C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(BasilEngine
    main.cpp
    AsciiCanvas.c
)

set(RAYLIB_INCLUDE_PATH "C:/raylib/raylib/src/build/include")
set(RAYLIB_LIB_PATH     "C:/raylib/raylib/src")

set(BTOOL_INCLUDE_PATH "C:/dev/BasilsTools/Include")
set(BTOOL_LIB_PATH     "C:/dev/BasilsTools/build")

add_library(imgui STATIC
    thirdparty/imgui/imgui.cpp
    thirdparty/imgui/imgui_draw.cpp
    thirdparty/imgui/imgui_tables.cpp
    thirdparty/imgui/imgui_widgets.cpp
    thirdparty/imgui/imgui_demo.cpp
)

target_include_directories(imgui PUBLIC
    thirdparty/imgui
)

add_library(rlimgui STATIC
    thirdparty/rlImGui/rlImGui.cpp
)

target_include_directories(rlimgui PUBLIC
    thirdparty/imgui
    thirdparty/rlImGui
    "${RAYLIB_INCLUDE_PATH}"
)

target_link_libraries(rlimgui PUBLIC
    imgui
    raylib
)

target_include_directories(BasilEngine PRIVATE
    "${RAYLIB_INCLUDE_PATH}"
    "${BTOOL_INCLUDE_PATH}"
    thirdparty/imgui
    thirdparty/rlImGui
)

target_link_directories(BasilEngine PRIVATE
    "${RAYLIB_LIB_PATH}"
    "${BTOOL_LIB_PATH}"
)

target_link_libraries(BasilEngine PRIVATE
    raylib
    winmm
    gdi32
    opengl32
    BasilsTools
    imgui
    rlimgui
)
