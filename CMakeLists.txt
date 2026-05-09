cmake_minimum_required(VERSION 3.20)

project(BasilEngine C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RAYLIB_INCLUDE_PATH "C:/raylib/raylib/src/build/include" CACHE PATH "Path to raylib include directory")
set(RAYLIB_LIB_PATH     "C:/raylib/raylib/src" CACHE PATH "Path to raylib library directory")

set(BTOOL_INCLUDE_PATH "C:/dev/BasilsTools/Include" CACHE PATH "Path to BasilsTools include directory")
set(BTOOL_LIB_PATH     "C:/dev/BasilsTools/build" CACHE PATH "Path to BasilsTools library directory")

add_library(BasilEngine STATIC
    engine/core/BEngine.c
    engine/core/BApplication.c
    engine/rendering/AsciiCanvas.c
    engine/core/BWindow.c
    engine/core/BTime.c 
    engine/core/BLog.c
    engine/debug/BConsole.c
)

target_include_directories(BasilEngine PUBLIC
    engine/core
    engine/rendering
    engine/debug
    "${RAYLIB_INCLUDE_PATH}"
    "${BTOOL_INCLUDE_PATH}"
)

target_link_directories(BasilEngine PUBLIC
    "${RAYLIB_LIB_PATH}"
    "${BTOOL_LIB_PATH}"
)

target_link_libraries(BasilEngine PUBLIC
    raylib
    winmm
    gdi32
    opengl32
    BasilsTools
)

add_executable(WhereBirdsNest
    projects/WhereBirdsNest/main.c
)

target_link_libraries(WhereBirdsNest PRIVATE
    BasilEngine
)

# Editor dependencies are kept available, but the editor target is not wired yet.
# These will become part of the Day 2/Day 3 editor bootstrap pass.
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
    thirdparty/rlimgui/rlImGui.cpp
)

target_include_directories(rlimgui PUBLIC
    thirdparty/imgui
    thirdparty/rlimgui
    "${RAYLIB_INCLUDE_PATH}"
)

target_link_libraries(rlimgui PUBLIC
    imgui
    raylib
)

