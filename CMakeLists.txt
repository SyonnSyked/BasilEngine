cmake_minimum_required(VERSION 3.20)

project(BasilEngine C CXX)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BASIL_BUILD_EDITOR_DEPS "Build the currently unwired ImGui editor dependencies" OFF)

set(BASIL_RAYLIB_ROOT "" CACHE PATH "Optional root directory of a raylib installation or source tree")
set(BASIL_TOOLS_ROOT "" CACHE PATH "Optional root directory of a BasilsTools source tree")

# Prefer packages that provide proper transitive platform dependencies. The
# explicit root hints support local source builds without baking one machine's
# directory layout into the project.
find_package(raylib CONFIG QUIET)

if(TARGET raylib)
    set(BASIL_RAYLIB_TARGET raylib)
elseif(TARGET raylib::raylib)
    set(BASIL_RAYLIB_TARGET raylib::raylib)
else()
    find_package(PkgConfig QUIET)

    if(PkgConfig_FOUND)
        pkg_check_modules(RAYLIB QUIET IMPORTED_TARGET raylib)
    endif()

    if(TARGET PkgConfig::RAYLIB)
        set(BASIL_RAYLIB_TARGET PkgConfig::RAYLIB)
    else()
        find_path(BASIL_RAYLIB_INCLUDE_DIR
            NAMES raylib.h
            HINTS
                "${BASIL_RAYLIB_ROOT}"
                "${RAYLIB_INCLUDE_PATH}"
            PATH_SUFFIXES include src build/include src/build/include
        )

        find_library(BASIL_RAYLIB_LIBRARY
            NAMES raylib libraylib
            HINTS
                "${BASIL_RAYLIB_ROOT}"
                "${RAYLIB_LIB_PATH}"
            PATH_SUFFIXES lib build src
        )

        if(NOT BASIL_RAYLIB_INCLUDE_DIR OR NOT BASIL_RAYLIB_LIBRARY)
            message(FATAL_ERROR
                "raylib was not found. Install a raylib CMake/pkg-config package "
                "or set BASIL_RAYLIB_ROOT to its installation/source directory."
            )
        endif()

        add_library(BasilRaylib UNKNOWN IMPORTED)
        set_target_properties(BasilRaylib PROPERTIES
            IMPORTED_LOCATION "${BASIL_RAYLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${BASIL_RAYLIB_INCLUDE_DIR}"
        )
        set(BASIL_RAYLIB_TARGET BasilRaylib)
        set(BASIL_RAYLIB_IS_RAW_ARCHIVE TRUE)
    endif()
endif()

find_package(BasilsTools CONFIG QUIET)

if(TARGET BasilsTools)
    set(BASIL_TOOLS_TARGET BasilsTools)
elseif(TARGET BasilsTools::BasilsTools)
    set(BASIL_TOOLS_TARGET BasilsTools::BasilsTools)
else()
    find_path(BASIL_TOOLS_INCLUDE_DIR
        NAMES BGrid.h BTile.h
        HINTS
            "${BASIL_TOOLS_ROOT}"
            "${BTOOL_INCLUDE_PATH}"
        PATH_SUFFIXES include Include
    )

    find_library(BASIL_TOOLS_LIBRARY
        NAMES BasilsTools libBasilsTools
        HINTS
            "${BASIL_TOOLS_ROOT}"
            "${BTOOL_LIB_PATH}"
        PATH_SUFFIXES lib build
    )

    if(NOT BASIL_TOOLS_INCLUDE_DIR OR NOT BASIL_TOOLS_LIBRARY)
        message(FATAL_ERROR
            "BasilsTools was not found. Install its CMake package or set "
            "BASIL_TOOLS_ROOT to its source directory."
        )
    endif()

    add_library(BasilToolsDependency UNKNOWN IMPORTED)
    set_target_properties(BasilToolsDependency PROPERTIES
        IMPORTED_LOCATION "${BASIL_TOOLS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BASIL_TOOLS_INCLUDE_DIR}"
    )
    set(BASIL_TOOLS_TARGET BasilToolsDependency)
endif()

add_library(BasilEngine STATIC
    engine/core/BEngine.c
    engine/core/BApplication.c
    engine/rendering/AsciiCanvas.c
    engine/core/BWindow.c
    engine/core/BTime.c 
    engine/core/BLog.c
    engine/debug/BConsole.c 
    engine/input/BInput.c
)

target_include_directories(BasilEngine PUBLIC
    engine/core
    engine/rendering
    engine/debug
    engine/input
)

target_link_libraries(BasilEngine PUBLIC
    ${BASIL_RAYLIB_TARGET}
    ${BASIL_TOOLS_TARGET}
)

# A raw static archive does not carry raylib's platform link requirements.
# Proper CMake and pkg-config targets provide these transitively.
if(BASIL_RAYLIB_IS_RAW_ARCHIVE)
    if(WIN32)
        target_link_libraries(BasilEngine PUBLIC winmm gdi32 opengl32)
    elseif(APPLE)
        target_link_libraries(BasilEngine PUBLIC
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
            "-framework OpenGL"
        )
    elseif(UNIX)
        find_package(Threads REQUIRED)
        find_package(X11 REQUIRED)
        target_link_libraries(BasilEngine PUBLIC m dl rt Threads::Threads X11::X11)
    endif()
endif()

if(MSVC)
    target_compile_options(BasilEngine PRIVATE /W4)
else()
    target_compile_options(BasilEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(WhereBirdsNest
    projects/wherebirdsnest/main.c
    projects/wherebirdsnest/WBNCombat.c
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets/wherebirdsnest")
configure_file(
    projects/wherebirdsnest/assets/arena.txt
    "${CMAKE_CURRENT_BINARY_DIR}/assets/wherebirdsnest/arena.txt"
    COPYONLY
)

target_link_libraries(WhereBirdsNest PRIVATE
    BasilEngine
)

if(BUILD_TESTING)
    add_executable(WBNCombatTests
        tests/WBNCombatTests.c
        projects/wherebirdsnest/WBNCombat.c
    )

    target_include_directories(WBNCombatTests PRIVATE
        projects/wherebirdsnest
    )

    add_test(NAME WBNCombatTests COMMAND WBNCombatTests)

    add_executable(AsciiCanvasTests
        tests/AsciiCanvasTests.c
    )

    target_link_libraries(AsciiCanvasTests PRIVATE
        BasilEngine
    )

    add_test(NAME AsciiCanvasTests COMMAND AsciiCanvasTests)

    add_executable(BInputTests
        tests/BInputTests.c
    )

    target_link_libraries(BInputTests PRIVATE
        BasilEngine
    )

    add_test(NAME BInputTests COMMAND BInputTests)
endif()

if(BASIL_BUILD_EDITOR_DEPS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui.cpp"
        OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rlimgui/rlImGui.cpp")
        message(FATAL_ERROR
            "BASIL_BUILD_EDITOR_DEPS requires thirdparty/imgui and thirdparty/rlimgui."
        )
    endif()

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
    )

    target_link_libraries(rlimgui PUBLIC
        imgui
        ${BASIL_RAYLIB_TARGET}
    )
endif()

