if(NOT PROJECT_TOOL OR NOT ENGINE_SOURCE_DIR OR NOT TEST_ROOT)
    message(FATAL_ERROR "Generated project test is missing required paths.")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")

foreach(language_mode IN ITEMS mixed c cpp)
    string(TOUPPER "${language_mode}" display_mode)
    set(identifier "Generated${display_mode}")

    execute_process(
        COMMAND "${PROJECT_TOOL}" create "Generated ${display_mode} Project"
            "${identifier}" "${TEST_ROOT}" --language "${language_mode}"
        RESULT_VARIABLE create_result
        OUTPUT_VARIABLE create_output
        ERROR_VARIABLE create_error
    )

    if(NOT create_result EQUAL 0)
        message(FATAL_ERROR "${language_mode} project generation failed:\n${create_output}\n${create_error}")
    endif()

    set(source_directory "${TEST_ROOT}/${identifier}")
    set(build_directory "${source_directory}/build")
    set(configure_command
        "${CMAKE_COMMAND}"
        -S "${source_directory}"
        -B "${build_directory}"
        -G "${TEST_GENERATOR}"
        "-DBASIL_ENGINE_ROOT=${ENGINE_SOURCE_DIR}"
        "-DBASIL_RAYLIB_ROOT=${RAYLIB_ROOT}"
        "-DBASIL_TOOLS_ROOT=${TOOLS_ROOT}"
        "-DBASIL_RAYLIB_INCLUDE_DIR=${RAYLIB_INCLUDE_DIR}"
        "-DBASIL_RAYLIB_LIBRARY=${RAYLIB_LIBRARY}"
        "-DBASIL_TOOLS_INCLUDE_DIR=${TOOLS_INCLUDE_DIR}"
        "-DBASIL_TOOLS_LIBRARY=${TOOLS_LIBRARY}"
        "-DCMAKE_C_COMPILER=${TEST_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
    )

    if(TEST_TOOLCHAIN_FILE)
        list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${TEST_TOOLCHAIN_FILE}")
    endif()

    execute_process(
        COMMAND ${configure_command}
        RESULT_VARIABLE configure_result
        OUTPUT_VARIABLE configure_output
        ERROR_VARIABLE configure_error
    )

    if(NOT configure_result EQUAL 0)
        message(FATAL_ERROR "${language_mode} project configuration failed:\n${configure_output}\n${configure_error}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build_directory}"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error
    )

    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "${language_mode} project build failed:\n${build_output}\n${build_error}")
    endif()
endforeach()
