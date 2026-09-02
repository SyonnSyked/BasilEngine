if(NOT ENGINE_SOURCE_DIR OR NOT REFERENCE_PROJECT OR NOT TEST_ROOT)
    message(FATAL_ERROR "Where Birds Nest Project test is missing required paths.")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
set(relocated "${TEST_ROOT}/Where Birds Nest Relocated")
file(MAKE_DIRECTORY "${relocated}")
file(COPY "${REFERENCE_PROJECT}/" DESTINATION "${relocated}")
set(build_directory "${relocated}/build")
set(configure_command
    "${CMAKE_COMMAND}" -S "${relocated}" -B "${build_directory}"
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
execute_process(COMMAND ${configure_command}
    RESULT_VARIABLE configure_result OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Where Birds Nest configuration failed:\n${configure_output}\n${configure_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${build_directory}"
    RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Where Birds Nest build failed:\n${build_output}\n${build_error}")
endif()

if(WIN32)
    set(executable "${build_directory}/WhereBirdsNest.exe")
else()
    set(executable "${build_directory}/WhereBirdsNest")
endif()
execute_process(
    COMMAND "${executable}" --basil-validate --project "${REFERENCE_PROJECT}/WhereBirdsNest.basilproject"
    WORKING_DIRECTORY "${REFERENCE_PROJECT}"
    RESULT_VARIABLE explicit_result OUTPUT_VARIABLE explicit_output ERROR_VARIABLE explicit_error
)
if(NOT explicit_result EQUAL 0 OR NOT explicit_output MATCHES "project=WhereBirdsNest.*items=1163")
    message(FATAL_ERROR "In-place Where Birds Nest validation failed:\n${explicit_output}\n${explicit_error}")
endif()
execute_process(
    COMMAND "${executable}" --basil-validate
    WORKING_DIRECTORY "${build_directory}"
    RESULT_VARIABLE relocated_result OUTPUT_VARIABLE relocated_output ERROR_VARIABLE relocated_error
)
if(NOT relocated_result EQUAL 0 OR NOT relocated_output MATCHES "project=WhereBirdsNest.*items=1163")
    message(FATAL_ERROR "Relocated Where Birds Nest validation failed:\n${relocated_output}\n${relocated_error}")
endif()
