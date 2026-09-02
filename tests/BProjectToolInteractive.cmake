if(NOT PROJECT_TOOL OR NOT TEST_ROOT OR NOT INPUT_FILE)
    message(FATAL_ERROR "Interactive project-tool test is missing required paths.")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")

execute_process(
    COMMAND "${PROJECT_TOOL}"
    WORKING_DIRECTORY "${TEST_ROOT}"
    INPUT_FILE "${INPUT_FILE}"
    RESULT_VARIABLE tool_result
    OUTPUT_VARIABLE tool_output
    ERROR_VARIABLE tool_error
)

if(NOT tool_result EQUAL 0)
    message(FATAL_ERROR "Interactive project creation failed:\n${tool_output}\n${tool_error}")
endif()

if(NOT EXISTS "${TEST_ROOT}/InteractiveTest/InteractiveTest.basilproject")
    message(FATAL_ERROR "Interactive project creator did not write its manifest.")
endif()

if(NOT tool_output MATCHES "Created project 'Interactive Test Project'")
    message(FATAL_ERROR "Interactive project creator did not report success:\n${tool_output}")
endif()
