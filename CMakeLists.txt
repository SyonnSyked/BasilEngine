cmake_minimum_required(VERSION 3.20)

project(BasilEngine C)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BASIL_BUILD_EDITOR_DEPS "Build BasilEditor and its ImGui dependencies" OFF)
option(BASIL_BUILD_PROJECT_TOOL "Build the headless BasilEngine project tool" ON)
option(BASIL_BUILD_REFERENCE_GAME "Build the Where Birds Nest reference game" ON)

function(basil_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

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

add_library(cjson STATIC
    thirdparty/cjson/cJSON.c
)

target_include_directories(cjson PUBLIC
    thirdparty/cjson
)

add_library(BasilProject STATIC
    engine/project/BProject.c
    engine/project/BProjectGenerator.c
    engine/project/BRecentProjects.c
)

target_include_directories(BasilProject PUBLIC
    engine/project
)

target_link_libraries(BasilProject PRIVATE
    cjson
)

basil_enable_warnings(BasilProject)

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

basil_enable_warnings(BasilEngine)

if(BASIL_BUILD_PROJECT_TOOL)
    add_executable(BasilProjectTool
        tools/project/BProjectTool.c
    )

    target_link_libraries(BasilProjectTool PRIVATE
        BasilProject
    )

    basil_enable_warnings(BasilProjectTool)
endif()

if(BASIL_BUILD_REFERENCE_GAME)
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
endif()

if(BUILD_TESTING)
    add_executable(BProjectTests
        tests/BProjectTests.c
    )

    target_link_libraries(BProjectTests PRIVATE
        BasilProject
    )

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures")
    configure_file(
        tests/fixtures/project-invalid-json.basilproject
        "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/project-invalid-json.basilproject"
        COPYONLY
    )
    configure_file(
        tests/fixtures/project-fractional-standard.basilproject
        "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/project-fractional-standard.basilproject"
        COPYONLY
    )
    configure_file(
        tests/fixtures/project-unsupported-version.basilproject
        "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/project-unsupported-version.basilproject"
        COPYONLY
    )

    add_test(NAME BProjectTests COMMAND BProjectTests)

    add_executable(BRecentProjectsTests
        tests/BRecentProjectsTests.c
    )

    target_link_libraries(BRecentProjectsTests PRIVATE
        BasilProject
    )

    add_test(NAME BRecentProjectsTests COMMAND BRecentProjectsTests)

    if(BASIL_BUILD_PROJECT_TOOL)
        add_test(
            NAME BProjectToolInteractive
            COMMAND ${CMAKE_COMMAND}
                -DPROJECT_TOOL=$<TARGET_FILE:BasilProjectTool>
                -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/interactive-project-tool-test
                -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/project-tool-interactive-input.txt
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BProjectToolInteractive.cmake
        )

        add_test(
            NAME GeneratedProjectBuild
            COMMAND ${CMAKE_COMMAND}
                -DPROJECT_TOOL=$<TARGET_FILE:BasilProjectTool>
                -DENGINE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/generated-project-test
                -DTEST_GENERATOR=${CMAKE_GENERATOR}
                -DTEST_C_COMPILER=${CMAKE_C_COMPILER}
                -DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -DTEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                -DRAYLIB_ROOT=${BASIL_RAYLIB_ROOT}
                -DTOOLS_ROOT=${BASIL_TOOLS_ROOT}
                -DRAYLIB_INCLUDE_DIR=${BASIL_RAYLIB_INCLUDE_DIR}
                -DRAYLIB_LIBRARY=${BASIL_RAYLIB_LIBRARY}
                -DTOOLS_INCLUDE_DIR=${BASIL_TOOLS_INCLUDE_DIR}
                -DTOOLS_LIBRARY=${BASIL_TOOLS_LIBRARY}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/GeneratedProjectBuild.cmake
        )
    endif()

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
    enable_language(CXX)

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

    add_executable(BasilEditor
        editor/BasilEditor.cpp
        editor/BEditorPreferences.cpp
        editor/BEditorTheme.cpp
    )

    if(WIN32)
        enable_language(RC)
        target_sources(BasilEditor PRIVATE
            editor/windows/BasilEditor.rc
        )
    endif()

    target_link_libraries(BasilEditor PRIVATE
        BasilProject
        BasilEngine
        rlimgui
        cjson
    )

    # Current MSYS2 binutils can fail silently while linking the combined
    # raylib/ImGui editor. Prefer LLVM's compatible MinGW linker when present.
    if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Avoid resolving an incompatible libstdc++/libgcc from another MinGW
        # installation (Git for Windows commonly places one earlier on PATH).
        target_link_options(BasilEditor PRIVATE -static -static-libgcc -static-libstdc++)

        find_program(BASIL_LLD_LINKER NAMES ld.lld lld)
        if(BASIL_LLD_LINKER)
            target_link_options(BasilEditor PRIVATE -fuse-ld=lld)
        endif()
    endif()

    basil_enable_warnings(BasilEditor)

    if(BUILD_TESTING)
        add_executable(BEditorPreferencesTests
            tests/BEditorPreferencesTests.cpp
            editor/BEditorPreferences.cpp
        )

        target_include_directories(BEditorPreferencesTests PRIVATE
            editor
        )

        target_link_libraries(BEditorPreferencesTests PRIVATE
            cjson
        )

        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorPreferencesTests PRIVATE
                -static -static-libgcc -static-libstdc++
            )
        endif()

        basil_enable_warnings(BEditorPreferencesTests)
        add_test(NAME BEditorPreferencesTests COMMAND BEditorPreferencesTests)
    endif()

    add_custom_command(TARGET BasilEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:BasilEditor>/assets/editor/fonts/JetBrainsMono"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:BasilEditor>/assets/editor/branding/icons"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/fonts/JetBrainsMono/JetBrainsMono-Regular.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/fonts/JetBrainsMono/JetBrainsMono-Bold.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/fonts/JetBrainsMono/OFL.txt"
            "$<TARGET_FILE_DIR:BasilEditor>/assets/editor/fonts/JetBrainsMono"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-16.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-24.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-32.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-48.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-64.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-128.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-256.png"
            "$<TARGET_FILE_DIR:BasilEditor>/assets/editor/branding/icons"
        VERBATIM
    )
endif()

