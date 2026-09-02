cmake_minimum_required(VERSION 3.25)

project(WhereBirdsNest LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(BASIL_ENGINE_ROOT "" CACHE PATH "Path to the BasilEngine source tree")

if(NOT BASIL_ENGINE_ROOT)
    message(FATAL_ERROR "Set BASIL_ENGINE_ROOT to the BasilEngine source tree.")
endif()

set(BASIL_BUILD_REFERENCE_GAME OFF CACHE BOOL "" FORCE)
set(BASIL_BUILD_PROJECT_TOOL OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${BASIL_ENGINE_ROOT}" "${CMAKE_BINARY_DIR}/_basilengine")

add_executable(WhereBirdsNest source/main.c)
add_library(WhereBirdsNestGame MODULE source/game.c)
set_target_properties(WhereBirdsNestGame PROPERTIES PREFIX "" OUTPUT_NAME "WhereBirdsNest.candidate")
target_link_libraries(WhereBirdsNest PRIVATE BasilEngine)
target_link_libraries(WhereBirdsNestGame PRIVATE BasilGameAPI)
add_dependencies(WhereBirdsNest WhereBirdsNestGame)
add_custom_command(TARGET WhereBirdsNestGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:WhereBirdsNestGame>"
        "$<TARGET_FILE_DIR:WhereBirdsNest>/WhereBirdsNest.game${CMAKE_SHARED_MODULE_SUFFIX}"
)
