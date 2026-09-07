cmake_minimum_required(VERSION 3.25)

project(BasilEngine C)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BASIL_BUILD_EDITOR_DEPS "Build BasilEditor and its ImGui dependencies" OFF)
option(BASIL_BUILD_PROJECT_TOOL "Build the headless BasilEngine project tool" ON)
option(BASIL_ENABLE_GLM "Provide GLM for C++ game Project targets" OFF)
option(BASIL_FETCH_GLM "Fetch the pinned GLM release when no local package is available" OFF)
set(BASIL_GLM_ROOT "" CACHE PATH "Optional GLM installation or source-tree root")

function(basil_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

set(BASIL_RAYLIB_ROOT
    ""
    CACHE PATH "Optional root directory of a raylib installation or source tree")

# Prefer packages that provide proper transitive platform dependencies. The explicit root hints
# support local source builds without baking one machine's directory layout into the project.
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
        find_path(
            BASIL_RAYLIB_INCLUDE_DIR
            NAMES raylib.h
            HINTS "${BASIL_RAYLIB_ROOT}" "${RAYLIB_INCLUDE_PATH}"
            PATH_SUFFIXES include src build/include src/build/include)

        find_library(
            BASIL_RAYLIB_LIBRARY
            NAMES raylib libraylib
            HINTS "${BASIL_RAYLIB_ROOT}" "${RAYLIB_LIB_PATH}"
            PATH_SUFFIXES lib build src)

        if(NOT BASIL_RAYLIB_INCLUDE_DIR OR NOT BASIL_RAYLIB_LIBRARY)
            message(FATAL_ERROR "raylib was not found. Install a raylib CMake/pkg-config package "
                                "or set BASIL_RAYLIB_ROOT to its installation/source directory.")
        endif()

        add_library(BasilRaylib UNKNOWN IMPORTED)
        set_target_properties(
            BasilRaylib PROPERTIES IMPORTED_LOCATION "${BASIL_RAYLIB_LIBRARY}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${BASIL_RAYLIB_INCLUDE_DIR}")
        set(BASIL_RAYLIB_TARGET BasilRaylib)
        set(BASIL_RAYLIB_IS_RAW_ARCHIVE TRUE)
    endif()
endif()

add_library(cjson STATIC thirdparty/cjson/cJSON.c)

target_include_directories(cjson PUBLIC thirdparty/cjson)

add_library(
    BasilProject STATIC
    engine/project/BProject.c engine/project/BProjectGenerator.c engine/project/BRecentProjects.c
    engine/project/BWorkspace.c engine/project/BAssetRegistry.c engine/project/BAssetRef.c)

target_include_directories(BasilProject PUBLIC engine/project engine/core)

target_link_libraries(BasilProject PRIVATE cjson)

basil_enable_warnings(BasilProject)

add_library(BasilAssets STATIC engine/assets/BTextSprite.c)

target_include_directories(BasilAssets PUBLIC engine/assets engine/core)

basil_enable_warnings(BasilAssets)

add_library(
    BasilRuntimeWorkspace STATIC engine/runtime/BAsciiDrawList.c engine/runtime/BCollision2D.c
                                 engine/runtime/BGameUI.c
                                 engine/runtime/BProjectContext.c
                                 engine/runtime/BWorkspaceReplacement.c)

target_include_directories(BasilRuntimeWorkspace PUBLIC engine/runtime)

target_link_libraries(BasilRuntimeWorkspace PUBLIC BasilProject BasilAssets)

basil_enable_warnings(BasilRuntimeWorkspace)

add_library(
    BasilEngine STATIC
    engine/core/BEngine.c
    engine/core/BApplication.c
    engine/core/BWindow.c
    engine/core/BTime.c
    engine/core/BLog.c
    engine/input/BInput.c
    engine/runtime/BGeneratedRuntime.c
    engine/runtime/BDynamicLibrary.c)

add_library(BasilGameAPI INTERFACE)
target_include_directories(BasilGameAPI INTERFACE engine/runtime)

if(BASIL_ENABLE_GLM)
    find_package(glm 1.0.3 EXACT CONFIG QUIET HINTS "${BASIL_GLM_ROOT}")

    if(NOT TARGET glm::glm AND BASIL_GLM_ROOT AND EXISTS "${BASIL_GLM_ROOT}/CMakeLists.txt")
        set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${BASIL_GLM_ROOT}" "${CMAKE_BINARY_DIR}/_basil_glm" EXCLUDE_FROM_ALL)
    endif()

    if(NOT TARGET glm::glm AND BASIL_FETCH_GLM)
        include(FetchContent)
        set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            basil_glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 8d1fd52e5ab5590e2c81768ace50c72bae28f2ed
            GIT_SHALLOW FALSE)
        FetchContent_MakeAvailable(basil_glm)
    endif()

    if(NOT TARGET glm::glm)
        message(FATAL_ERROR
                "GLM 1.0.3 was requested but could not be resolved. Install GLM 1.0.3, set BASIL_GLM_ROOT "
                "to a GLM install/source tree, or enable BASIL_FETCH_GLM.")
    endif()

    add_library(BasilGLM INTERFACE)
    target_link_libraries(BasilGLM INTERFACE glm::glm)
endif()

target_include_directories(BasilEngine PUBLIC engine/core engine/rendering engine/debug
                                              engine/input engine/runtime)

target_link_libraries(BasilEngine PUBLIC BasilRuntimeWorkspace ${BASIL_RAYLIB_TARGET}
                                         ${CMAKE_DL_LIBS})
target_link_libraries(BasilEngine PRIVATE cjson)

# A raw static archive does not carry raylib's platform link requirements. Proper CMake and
# pkg-config targets provide these transitively.
if(BASIL_RAYLIB_IS_RAW_ARCHIVE)
    if(WIN32)
        target_link_libraries(BasilEngine PUBLIC winmm gdi32 opengl32)
    elseif(APPLE)
        target_link_libraries(BasilEngine PUBLIC "-framework Cocoa" "-framework IOKit"
                                                 "-framework CoreVideo" "-framework OpenGL")
    elseif(UNIX)
        find_package(Threads REQUIRED)
        find_package(X11 REQUIRED)
        target_link_libraries(BasilEngine PUBLIC m dl rt Threads::Threads X11::X11)
    endif()
endif()

basil_enable_warnings(BasilEngine)

if(BASIL_BUILD_PROJECT_TOOL)
    add_executable(BasilProjectTool tools/project/BProjectTool.c)

    target_link_libraries(BasilProjectTool PRIVATE BasilProject)

    basil_enable_warnings(BasilProjectTool)
endif()

if(BUILD_TESTING)
    add_executable(BProjectTests tests/BProjectTests.c)

    target_link_libraries(BProjectTests PRIVATE BasilProject)

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures")
    configure_file(
        tests/fixtures/project-invalid-json.basilproject
        "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/project-invalid-json.basilproject" COPYONLY)
    configure_file(
        tests/fixtures/project-fractional-standard.basilproject
        "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/project-fractional-standard.basilproject"
        COPYONLY)
    configure_file(
        tests/fixtures/project-unsupported-version.basilproject
        "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/project-unsupported-version.basilproject"
        COPYONLY)
    configure_file(
        tests/fixtures/project-legacy-version.basilproject
        "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/project-legacy-version.basilproject" COPYONLY)

    add_test(NAME BProjectTests COMMAND BProjectTests)

    add_executable(BWorkspaceTests tests/BWorkspaceTests.c)

    target_link_libraries(BWorkspaceTests PRIVATE BasilProject)

    foreach(
        workspace_fixture IN
        ITEMS workspace-invalid-json.basilworkspace
              workspace-unsupported-version.basilworkspace
              workspace-nonempty-entities.basilworkspace
              workspace-legacy-empty.basilworkspace
              workspace-unknown-field.basilworkspace
              workspace-components-v3.basilworkspace
              workspace-components-v4.basilworkspace
              workspace-required-unknown-v3.basilworkspace
              workspace-entities-v2.basilworkspace
              workspace-duplicate-components-v3.basilworkspace)
        configure_file("tests/fixtures/${workspace_fixture}"
                       "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/${workspace_fixture}" COPYONLY)
    endforeach()

    add_test(NAME BWorkspaceTests COMMAND BWorkspaceTests)

    add_executable(BAssetRegistryTests tests/BAssetRegistryTests.c)

    target_link_libraries(BAssetRegistryTests PRIVATE BasilProject)

    foreach(asset_registry_fixture IN
            ITEMS asset-registry-valid.json asset-registry-duplicate-id.json
                  asset-registry-duplicate-path.json)
        configure_file(
            "tests/fixtures/${asset_registry_fixture}"
            "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures/${asset_registry_fixture}" COPYONLY)
    endforeach()

    add_test(NAME BAssetRegistryTests COMMAND BAssetRegistryTests)

    add_executable(BTextSpriteTests tests/BTextSpriteTests.c)

    target_link_libraries(BTextSpriteTests PRIVATE BasilAssets)

    add_test(NAME BTextSpriteTests COMMAND BTextSpriteTests)

    add_executable(BAsciiDrawListTests tests/BAsciiDrawListTests.c)

    target_link_libraries(BAsciiDrawListTests PRIVATE BasilRuntimeWorkspace)

    add_test(NAME BAsciiDrawListTests COMMAND BAsciiDrawListTests)

    add_executable(BGameUITests tests/BGameUITests.c)
    target_link_libraries(BGameUITests PRIVATE BasilRuntimeWorkspace)
    basil_enable_warnings(BGameUITests)
    add_test(NAME BGameUITests COMMAND BGameUITests)

    add_executable(BGameModuleGlueTests tests/BGameModuleGlueTests.c
                                            engine/runtime/BGameModuleGlue.c)
    target_link_libraries(BGameModuleGlueTests PRIVATE BasilGameAPI)
    basil_enable_warnings(BGameModuleGlueTests)
    add_test(NAME BGameModuleGlueTests COMMAND BGameModuleGlueTests)

    add_executable(BWorkspaceReplacementTests tests/BWorkspaceReplacementTests.c)
    target_link_libraries(BWorkspaceReplacementTests PRIVATE BasilRuntimeWorkspace)
    basil_enable_warnings(BWorkspaceReplacementTests)
    add_test(NAME BWorkspaceReplacementTests COMMAND BWorkspaceReplacementTests)

    add_executable(BCollision2DTests tests/BCollision2DTests.c)
    target_link_libraries(BCollision2DTests PRIVATE BasilRuntimeWorkspace)
    basil_enable_warnings(BCollision2DTests)
    add_test(NAME BCollision2DTests COMMAND BCollision2DTests)

    add_executable(BRecentProjectsTests tests/BRecentProjectsTests.c)

    target_link_libraries(BRecentProjectsTests PRIVATE BasilProject)

    add_test(NAME BRecentProjectsTests COMMAND BRecentProjectsTests)

    if(BASIL_BUILD_PROJECT_TOOL)
        add_test(
            NAME BProjectToolInteractive
            COMMAND
                ${CMAKE_COMMAND} -DPROJECT_TOOL=$<TARGET_FILE:BasilProjectTool>
                -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/interactive-project-tool-test
                -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/project-tool-interactive-input.txt
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/BProjectToolInteractive.cmake)

        add_test(
            NAME GeneratedProjectBuild
            COMMAND
                ${CMAKE_COMMAND} -DPROJECT_TOOL=$<TARGET_FILE:BasilProjectTool>
                -DENGINE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/generated-project-test
                -DTEST_GENERATOR=${CMAKE_GENERATOR} -DTEST_C_COMPILER=${CMAKE_C_COMPILER}
                -DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -DTEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DRAYLIB_ROOT=${BASIL_RAYLIB_ROOT}
                -DTEST_GLM_ROOT=${BASIL_GLM_ROOT}
                -DRAYLIB_INCLUDE_DIR=${BASIL_RAYLIB_INCLUDE_DIR}
                -DRAYLIB_LIBRARY=${BASIL_RAYLIB_LIBRARY} -P
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/GeneratedProjectBuild.cmake)
    endif()

    add_test(
        NAME WhereBirdsNestProject
        COMMAND
            ${CMAKE_COMMAND} -DENGINE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DREFERENCE_PROJECT=${CMAKE_CURRENT_SOURCE_DIR}/projects/wherebirdsnest
            -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/where-birds-nest-project-test
            -DTEST_GENERATOR=${CMAKE_GENERATOR} -DTEST_C_COMPILER=${CMAKE_C_COMPILER}
            -DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DTEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DRAYLIB_ROOT=${BASIL_RAYLIB_ROOT}
            -DRAYLIB_INCLUDE_DIR=${BASIL_RAYLIB_INCLUDE_DIR}
            -DRAYLIB_LIBRARY=${BASIL_RAYLIB_LIBRARY} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/WhereBirdsNestProject.cmake)

    add_executable(WBNCombatTests tests/WBNCombatTests.c projects/wherebirdsnest/WBNCombat.c)

    target_include_directories(WBNCombatTests PRIVATE projects/wherebirdsnest)

    add_test(NAME WBNCombatTests COMMAND WBNCombatTests)

    add_executable(WBNGameplayTests tests/WBNGameplayTests.c
                                    projects/wherebirdsnest/source/WBNGameplay.c)
    target_include_directories(WBNGameplayTests PRIVATE projects/wherebirdsnest/source
                                                       engine/runtime)
    basil_enable_warnings(WBNGameplayTests)
    add_test(NAME WBNGameplayTests COMMAND WBNGameplayTests)

    add_executable(WBNGameIntegrationTests tests/WBNGameIntegrationTests.c
        projects/wherebirdsnest/source/game.c projects/wherebirdsnest/source/WBNGameplay.c)
    target_include_directories(WBNGameIntegrationTests PRIVATE projects/wherebirdsnest/source
                                                               engine/runtime)
    basil_enable_warnings(WBNGameIntegrationTests)
    add_test(NAME WBNGameIntegrationTests COMMAND WBNGameIntegrationTests)

    add_executable(BInputTests tests/BInputTests.c)

    target_link_libraries(BInputTests PRIVATE BasilEngine)

    add_test(NAME BInputTests COMMAND BInputTests)
endif()

if(BASIL_BUILD_EDITOR_DEPS)
    enable_language(CXX)

    configure_file(editor/BEditorBuildConfig.h.in
                   "${CMAKE_CURRENT_BINARY_DIR}/generated/BEditorBuildConfig.h" @ONLY)

    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui.cpp"
       OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rlimgui/rlImGui.cpp")
        message(
            FATAL_ERROR "BASIL_BUILD_EDITOR_DEPS requires thirdparty/imgui and thirdparty/rlimgui.")
    endif()

    add_library(
        imgui STATIC
        thirdparty/imgui/imgui.cpp thirdparty/imgui/imgui_draw.cpp
        thirdparty/imgui/imgui_tables.cpp thirdparty/imgui/imgui_widgets.cpp
        thirdparty/imgui/imgui_demo.cpp)

    target_include_directories(imgui PUBLIC thirdparty/imgui)

    add_library(rlimgui STATIC thirdparty/rlimgui/rlImGui.cpp)

    target_include_directories(rlimgui PUBLIC thirdparty/imgui thirdparty/rlimgui)

    target_link_libraries(rlimgui PUBLIC imgui ${BASIL_RAYLIB_TARGET})

    add_executable(
        BasilEditor
        editor/BasilEditor.cpp
        editor/BEditorBuildService.cpp
        editor/BEditorAssetService.cpp
        editor/BEditorComponentRegistry.cpp
        editor/BEditorCodeWorkspace.cpp
        editor/BEditorTerminalService.cpp
        editor/BEditorGit.cpp
        editor/BEditorPanels.cpp
        editor/BEditorPlatformDialogs.cpp
        editor/BEditorPreferences.cpp
        editor/BEditorTheme.cpp
        editor/BEditorTextSpriteDocument.cpp
        editor/BEditorUIConfig.cpp
        editor/BEditorWorkspaceSession.cpp)

    if(WIN32)
        enable_language(RC)
        target_sources(BasilEditor PRIVATE editor/windows/BasilEditor.rc)
    endif()

    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui.h" BASIL_IMGUI_DOCKING_MARKER
         REGEX "^#define IMGUI_HAS_DOCK")

    if(NOT BASIL_IMGUI_DOCKING_MARKER)
        message(
            FATAL_ERROR
                "BasilEditor requires the official Dear ImGui docking branch. "
                "Switch thirdparty/imgui to origin/docking or disable BASIL_BUILD_EDITOR_DEPS.")
    endif()

    target_link_libraries(BasilEditor PRIVATE BasilProject BasilEngine rlimgui cjson)

    if(WIN32)
        target_link_libraries(BasilEditor PRIVATE comdlg32 shell32 ole32)
    endif()

    target_include_directories(BasilEditor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

    # Current MSYS2 binutils can fail silently while linking the combined raylib/ImGui editor.
    # Prefer LLVM's compatible MinGW linker when present.
    if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Avoid resolving an incompatible libstdc++/libgcc from another MinGW installation (Git for
        # Windows commonly places one earlier on PATH).
        target_link_options(BasilEditor PRIVATE -static -static-libgcc -static-libstdc++)

        find_program(BASIL_LLD_LINKER NAMES ld.lld lld)
        if(BASIL_LLD_LINKER)
            target_link_options(BasilEditor PRIVATE -fuse-ld=lld)
        endif()
    endif()

    basil_enable_warnings(BasilEditor)

    if(BUILD_TESTING)
        add_executable(BEditorPreferencesTests tests/BEditorPreferencesTests.cpp
                                               editor/BEditorPreferences.cpp)

        target_include_directories(BEditorPreferencesTests PRIVATE editor)

        target_link_libraries(BEditorPreferencesTests PRIVATE cjson)

        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorPreferencesTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()

        basil_enable_warnings(BEditorPreferencesTests)
        add_test(NAME BEditorPreferencesTests COMMAND BEditorPreferencesTests)

        add_executable(BEditorGitTests tests/BEditorGitTests.cpp editor/BEditorGit.cpp)

        target_include_directories(BEditorGitTests PRIVATE editor)

        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorGitTests PRIVATE -static -static-libgcc -static-libstdc++)
        endif()

        basil_enable_warnings(BEditorGitTests)
        add_test(NAME BEditorGitTests COMMAND BEditorGitTests)

        add_executable(BEditorUIConfigTests tests/BEditorUIConfigTests.cpp
                                            editor/BEditorUIConfig.cpp)

        target_include_directories(BEditorUIConfigTests PRIVATE editor)

        target_link_libraries(BEditorUIConfigTests PRIVATE cjson)

        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorUIConfigTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()

        basil_enable_warnings(BEditorUIConfigTests)
        add_test(NAME BEditorUIConfigTests COMMAND BEditorUIConfigTests)

        add_executable(BEditorWorkspaceSessionTests tests/BEditorWorkspaceSessionTests.cpp
                                                    editor/BEditorWorkspaceSession.cpp)

        target_include_directories(BEditorWorkspaceSessionTests PRIVATE editor)

        target_link_libraries(BEditorWorkspaceSessionTests PRIVATE BasilRuntimeWorkspace)

        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorWorkspaceSessionTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()

        basil_enable_warnings(BEditorWorkspaceSessionTests)
        add_test(NAME BEditorWorkspaceSessionTests COMMAND BEditorWorkspaceSessionTests)

        add_executable(BEditorBuildServiceTests tests/BEditorBuildServiceTests.cpp
                                                editor/BEditorBuildService.cpp)

        target_include_directories(BEditorBuildServiceTests
                                   PRIVATE editor "${CMAKE_CURRENT_BINARY_DIR}/generated")

        target_link_libraries(BEditorBuildServiceTests PRIVATE BasilProject)

        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorBuildServiceTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()

        basil_enable_warnings(BEditorBuildServiceTests)
        add_test(NAME BEditorBuildServiceTests COMMAND BEditorBuildServiceTests)

        add_executable(BEditorAssetServiceTests tests/BEditorAssetServiceTests.cpp
                                                editor/BEditorAssetService.cpp)
        target_include_directories(BEditorAssetServiceTests PRIVATE editor)
        target_link_libraries(BEditorAssetServiceTests PRIVATE BasilProject)
        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorAssetServiceTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()
        basil_enable_warnings(BEditorAssetServiceTests)
        add_test(NAME BEditorAssetServiceTests COMMAND BEditorAssetServiceTests)

        add_executable(BEditorTextSpriteDocumentTests tests/BEditorTextSpriteDocumentTests.cpp
                                                      editor/BEditorTextSpriteDocument.cpp)
        target_include_directories(BEditorTextSpriteDocumentTests PRIVATE editor)
        target_link_libraries(BEditorTextSpriteDocumentTests PRIVATE BasilRuntimeWorkspace)
        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorTextSpriteDocumentTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()
        basil_enable_warnings(BEditorTextSpriteDocumentTests)
        add_test(NAME BEditorTextSpriteDocumentTests COMMAND BEditorTextSpriteDocumentTests)

        add_executable(BEditorComponentRegistryTests tests/BEditorComponentRegistryTests.cpp
                                                     editor/BEditorComponentRegistry.cpp)
        target_include_directories(BEditorComponentRegistryTests PRIVATE editor)
        target_link_libraries(BEditorComponentRegistryTests PRIVATE BasilRuntimeWorkspace cjson)
        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorComponentRegistryTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()
        basil_enable_warnings(BEditorComponentRegistryTests)
        add_test(NAME BEditorComponentRegistryTests COMMAND BEditorComponentRegistryTests)

        add_executable(BEditorCodeWorkspaceTests tests/BEditorCodeWorkspaceTests.cpp
                                                 editor/BEditorCodeWorkspace.cpp)
        target_include_directories(BEditorCodeWorkspaceTests PRIVATE editor)
        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorCodeWorkspaceTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()
        basil_enable_warnings(BEditorCodeWorkspaceTests)
        add_test(NAME BEditorCodeWorkspaceTests COMMAND BEditorCodeWorkspaceTests)

        add_executable(BEditorTerminalServiceTests tests/BEditorTerminalServiceTests.cpp
                                                   editor/BEditorTerminalService.cpp)
        target_include_directories(BEditorTerminalServiceTests PRIVATE editor)
        if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_link_options(BEditorTerminalServiceTests PRIVATE -static -static-libgcc
                                -static-libstdc++)
        endif()
        basil_enable_warnings(BEditorTerminalServiceTests)
        add_test(NAME BEditorTerminalServiceTests COMMAND BEditorTerminalServiceTests)
    endif()

    add_custom_command(
        TARGET BasilEditor
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:BasilEditor>/assets/editor/fonts/JetBrainsMono"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:BasilEditor>/assets/editor/branding/icons"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/fonts/JetBrainsMono/JetBrainsMono-Regular.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/fonts/JetBrainsMono/JetBrainsMono-Bold.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/fonts/JetBrainsMono/OFL.txt"
            "$<TARGET_FILE_DIR:BasilEditor>/assets/editor/fonts/JetBrainsMono"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-16.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-24.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-32.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-48.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-64.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-128.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/editor/branding/icons/basil-editor-256.png"
            "$<TARGET_FILE_DIR:BasilEditor>/assets/editor/branding/icons"
        VERBATIM)
endif()
