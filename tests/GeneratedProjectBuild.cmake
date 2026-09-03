if(NOT PROJECT_TOOL
   OR NOT ENGINE_SOURCE_DIR
   OR NOT TEST_ROOT)
    message(FATAL_ERROR "Generated project test is missing required paths.")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")

foreach(language_mode IN ITEMS mixed c cpp)
    string(TOUPPER "${language_mode}" display_mode)
    set(identifier "Generated${display_mode}")

    execute_process(
        COMMAND "${PROJECT_TOOL}" create "Generated ${display_mode} Project" "${identifier}"
                "${TEST_ROOT}" --language "${language_mode}"
        RESULT_VARIABLE create_result
        OUTPUT_VARIABLE create_output
        ERROR_VARIABLE create_error)

    if(NOT create_result EQUAL 0)
        message(
            FATAL_ERROR
                "${language_mode} project generation failed:\n${create_output}\n${create_error}")
    endif()

    set(source_directory "${TEST_ROOT}/${identifier}")
    set(build_directory "${source_directory}/build")
    if(language_mode STREQUAL "cpp")
        set(game_source "${source_directory}/source/game.cpp")
    else()
        set(game_source "${source_directory}/source/game.c")
    endif()

    file(READ "${game_source}" generated_game_source)

    set(input_api_smoke
        [=[
    bool (*inputPressedFn)(void*, const char*) = host->inputPressed;
    bool (*inputDownFn)(void*, const char*) = host->inputDown;
    bool (*inputReleasedFn)(void*, const char*) = host->inputReleased;
    bool (*inputRebindKeyboardFn)(void*, const char*, int) = host->inputRebindKeyboard;
    bool (*inputRebindMouseFn)(void*, const char*, int) = host->inputRebindMouse;
    bool (*inputHasActionFn)(void*, const char*) = host->inputHasAction;
    int (*inputBindingCodeFn)(void*, const char*) = host->inputBindingCode;
    int (*inputBindingDeviceFn)(void*, const char*) = host->inputBindingDevice;

    (void)inputPressedFn;
    (void)inputDownFn;
    (void)inputReleasedFn;
    (void)inputRebindKeyboardFn;
    (void)inputRebindMouseFn;
    (void)inputHasActionFn;
    (void)inputBindingCodeFn;
    (void)inputBindingDeviceFn;
]=])

    set(initialize_marker "    *gameState = &state;\n")

    string(REPLACE "${initialize_marker}" "${initialize_marker}${input_api_smoke}"
                   input_api_game_source "${generated_game_source}")

    if(input_api_game_source STREQUAL generated_game_source)
        message(
            FATAL_ERROR
                "${language_mode} generated game did not contain the expected GameState initialization marker"
        )
    endif()

    file(WRITE "${game_source}" "${input_api_game_source}")

    file(WRITE "${source_directory}/assets/ship.txt" "A \nBC\n")
    file(
        WRITE "${source_directory}/workspaces/Main.basilworkspace"
        "{\n"
        "  \"schemaVersion\": 3,\n"
        "  \"name\": \"Main Workspace\",\n"
        "  \"identifier\": \"Main\",\n"
        "  \"nextEntityId\": \"3\",\n"
        "  \"entities\": [\n"
        "    { \"id\": \"entity-0000000000000001\", \"name\": \"Glyph\", \"enabled\": true, \"components\": [\n"
        "      { \"type\": \"basil.transform2d\", \"version\": 1, \"required\": true, \"data\": { \"x\": 0, \"y\": 0 } },\n"
        "      { \"type\": \"basil.ascii-renderable\", \"version\": 1, \"required\": true, \"data\": { \"source\": { \"kind\": \"glyph\", \"glyph\": \"@\" }, \"foreground\": \"#00E5FFFF\", \"background\": \"#00000000\", \"layer\": 1, \"anchor\": \"bottom-center\", \"visible\": true, \"transparentSpaces\": true } }\n"
        "    ] },\n"
        "    { \"id\": \"entity-0000000000000002\", \"name\": \"Ship\", \"enabled\": true, \"components\": [\n"
        "      { \"type\": \"basil.transform2d\", \"version\": 1, \"required\": true, \"data\": { \"x\": 3.5, \"y\": -2 } },\n"
        "      { \"type\": \"basil.ascii-renderable\", \"version\": 1, \"required\": true, \"data\": { \"source\": { \"kind\": \"text-sprite\", \"path\": \"assets/ship.txt\" }, \"foreground\": \"#E6EDF3FF\", \"background\": \"#120C1FFF\", \"layer\": 0, \"anchor\": \"center\", \"visible\": true, \"transparentSpaces\": true } }\n"
        "    ] }\n"
        "  ]\n"
        "}\n")
    set(configure_command
        "${CMAKE_COMMAND}" -S "${source_directory}" -B "${build_directory}" -G "${TEST_GENERATOR}"
        "-DBASIL_ENGINE_ROOT=${ENGINE_SOURCE_DIR}" "-DBASIL_RAYLIB_ROOT=${RAYLIB_ROOT}"
        "-DBASIL_TOOLS_ROOT=${TOOLS_ROOT}" "-DBASIL_RAYLIB_INCLUDE_DIR=${RAYLIB_INCLUDE_DIR}"
        "-DBASIL_RAYLIB_LIBRARY=${RAYLIB_LIBRARY}" "-DBASIL_TOOLS_INCLUDE_DIR=${TOOLS_INCLUDE_DIR}"
        "-DBASIL_TOOLS_LIBRARY=${TOOLS_LIBRARY}" "-DCMAKE_C_COMPILER=${TEST_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}")

    if(TEST_TOOLCHAIN_FILE)
        list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${TEST_TOOLCHAIN_FILE}")
    endif()

    execute_process(
        COMMAND ${configure_command}
        RESULT_VARIABLE configure_result
        OUTPUT_VARIABLE configure_output
        ERROR_VARIABLE configure_error)

    if(NOT configure_result EQUAL 0)
        message(
            FATAL_ERROR
                "${language_mode} project configuration failed:\n${configure_output}\n${configure_error}"
        )
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build_directory}"
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error)

    if(NOT build_result EQUAL 0)
        message(
            FATAL_ERROR "${language_mode} project build failed:\n${build_output}\n${build_error}")
    endif()

    if(WIN32)
        set(project_executable "${build_directory}/${identifier}.exe")
    else()
        set(project_executable "${build_directory}/${identifier}")
    endif()
    if(WIN32)
        set(game_module "${build_directory}/${identifier}.game.dll")
    elseif(APPLE)
        set(game_module "${build_directory}/${identifier}.game.dylib")
    else()
        set(game_module "${build_directory}/${identifier}.game.so")
    endif()
    if(NOT EXISTS "${game_module}")
        message(FATAL_ERROR "${language_mode} build did not promote its game module")
    endif()

    file(READ "${game_source}" valid_game_source)
    file(SHA256 "${game_module}" valid_module_hash)
    file(APPEND "${game_source}" "\nthis intentionally does not compile\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build_directory}" --target "${identifier}Game"
        RESULT_VARIABLE failed_build_result
        OUTPUT_QUIET ERROR_QUIET)
    file(SHA256 "${game_module}" preserved_module_hash)
    if(failed_build_result EQUAL 0 OR NOT valid_module_hash STREQUAL preserved_module_hash)
        message(FATAL_ERROR "${language_mode} failed build did not preserve the last valid module")
    endif()

    string(REPLACE "module->version = BGAME_API_VERSION;"
                   "module->version = BGAME_API_VERSION + 1;" incompatible_game_source
                   "${valid_game_source}")
    file(WRITE "${game_source}" "${incompatible_game_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build_directory}" --target "${identifier}Game"
        RESULT_VARIABLE incompatible_build_result
        OUTPUT_QUIET ERROR_QUIET)
    execute_process(
        COMMAND "${project_executable}" --basil-validate --project
                "${source_directory}/${identifier}.basilproject"
        RESULT_VARIABLE incompatible_result
        OUTPUT_VARIABLE incompatible_output
        ERROR_VARIABLE incompatible_error)
    if(NOT incompatible_build_result EQUAL 0
       OR incompatible_result EQUAL 0
       OR NOT incompatible_error MATCHES "API mismatch: host requires 1, module provided 2")
        message(
            FATAL_ERROR
                "${language_mode} incompatible module was not rejected clearly:\n${incompatible_output}\n${incompatible_error}"
        )
    endif()
    file(WRITE "${game_source}" "${valid_game_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build_directory}" --target "${identifier}Game"
        RESULT_VARIABLE restore_build_result
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT restore_build_result EQUAL 0)
        message(FATAL_ERROR "${language_mode} valid module restoration failed")
    endif()

    set(manifest "${source_directory}/${identifier}.basilproject")
    execute_process(
        COMMAND "${project_executable}" --basil-validate --project "${manifest}"
        WORKING_DIRECTORY "${source_directory}"
        RESULT_VARIABLE validate_result
        OUTPUT_VARIABLE validate_output
        ERROR_VARIABLE validate_error)
    if(NOT validate_result EQUAL 0 OR NOT validate_output MATCHES "BASIL_RUNTIME_READY.*items=4")
        message(
            FATAL_ERROR
                "${language_mode} explicit runtime validation failed:\n${validate_output}\n${validate_error}"
        )
    endif()

    execute_process(
        COMMAND "${project_executable}" --basil-validate
        WORKING_DIRECTORY "${build_directory}"
        RESULT_VARIABLE direct_result
        OUTPUT_VARIABLE direct_output
        ERROR_VARIABLE direct_error)
    if(NOT direct_result EQUAL 0 OR NOT direct_output MATCHES "items=4")
        message(
            FATAL_ERROR
                "${language_mode} direct discovery failed:\n${direct_output}\n${direct_error}")
    endif()

    set(relocated_directory "${TEST_ROOT}/${identifier} Relocated")
    file(RENAME "${source_directory}" "${relocated_directory}")
    set(source_directory "${relocated_directory}")
    set(build_directory "${source_directory}/build")
    if(WIN32)
        set(project_executable "${build_directory}/${identifier}.exe")
    else()
        set(project_executable "${build_directory}/${identifier}")
    endif()
    execute_process(
        COMMAND "${project_executable}" --basil-validate
        WORKING_DIRECTORY "${build_directory}"
        RESULT_VARIABLE relocate_result
        OUTPUT_VARIABLE relocate_output
        ERROR_VARIABLE relocate_error)
    if(NOT relocate_result EQUAL 0 OR NOT relocate_output MATCHES "items=4")
        message(
            FATAL_ERROR
                "${language_mode} relocated discovery failed:\n${relocate_output}\n${relocate_error}"
        )
    endif()

    file(RENAME "${source_directory}/workspaces/Main.basilworkspace"
         "${source_directory}/workspaces/Main.missing")
    execute_process(
        COMMAND "${project_executable}" --basil-validate
        WORKING_DIRECTORY "${build_directory}"
        RESULT_VARIABLE missing_result
        OUTPUT_VARIABLE missing_output
        ERROR_VARIABLE missing_error)
    if(missing_result EQUAL 0 OR NOT missing_error MATCHES "BASIL_RUNTIME_ERROR")
        message(FATAL_ERROR "${language_mode} missing Workspace did not fail cleanly")
    endif()
    file(RENAME "${source_directory}/workspaces/Main.missing"
         "${source_directory}/workspaces/Main.basilworkspace")

    file(WRITE "${source_directory}/assets/ship.txt" "bad\tasset\n")
    execute_process(
        COMMAND "${project_executable}" --basil-validate
        WORKING_DIRECTORY "${build_directory}"
        RESULT_VARIABLE malformed_result
        OUTPUT_VARIABLE malformed_output
        ERROR_VARIABLE malformed_error)
    if(malformed_result EQUAL 0 OR NOT malformed_error MATCHES "BASIL_RUNTIME_ERROR")
        message(FATAL_ERROR "${language_mode} malformed Text Sprite did not fail cleanly")
    endif()
endforeach()
