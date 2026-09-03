#include "BProjectGenerator.h"
#include "BWorkspace.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define BPROJECT_MKDIR(path) _mkdir(path)
#else
#define BPROJECT_MKDIR(path) mkdir(path, 0755)
#endif

static bool BProjectGenerator_Fail(BProjectError *error, BProjectErrorCode code,
                                   const char *message)
{
    if (error != 0) {
        error->code = code;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }

    return false;
}

static bool BProjectGenerator_WorkspaceFailure(BProjectError *error,
                                               const BDiagnosticList *diagnostics)
{
    const BDiagnostic *diagnostic = BDiagnosticList_FirstError(diagnostics);
    BProjectErrorCode code = BPROJECT_ERROR_INVALID_MANIFEST;

    if (diagnostic != 0) {
        if (diagnostic->code == BDIAGNOSTIC_IO || diagnostic->code == BDIAGNOSTIC_OUT_OF_MEMORY)
            code = BPROJECT_ERROR_IO;
        else if (diagnostic->code == BDIAGNOSTIC_INVALID_ARGUMENT)
            code = BPROJECT_ERROR_INVALID_ARGUMENT;
        else if (diagnostic->code == BDIAGNOSTIC_UNSUPPORTED_VERSION)
            code = BPROJECT_ERROR_UNSUPPORTED_VERSION;
    }

    return BProjectGenerator_Fail(
        error, code, diagnostic != 0 ? diagnostic->message : "Workspace generation failed.");
}

static bool BProjectGenerator_Path(char *output, size_t outputSize, const char *left,
                                   const char *right, BProjectError *error)
{
    int written = snprintf(output, outputSize, "%s/%s", left, right);

    if (written < 0 || (size_t)written >= outputSize)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT,
                                      "Generated project path is too long.");

    return true;
}

static bool BProjectGenerator_PathExists(const char *path)
{
    struct stat information;
    return stat(path, &information) == 0;
}

static bool BProjectGenerator_CreateDirectory(const char *path, BProjectError *error)
{
    if (BPROJECT_MKDIR(path) == 0)
        return true;

    if (errno == EEXIST)
        return true;

    return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                  "Could not create a project directory.");
}

static bool BProjectGenerator_WriteText(const char *path, const char *contents,
                                        BProjectError *error)
{
    FILE *file = fopen(path, "wb");

    if (file == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                      "Could not create a generated project file.");

    size_t length = strlen(contents);
    bool succeeded = fwrite(contents, 1, length, file) == length;
    fclose(file);

    if (!succeeded)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                      "Could not write a complete generated project file.");

    return true;
}

static bool BProjectGenerator_WriteCMake(const BProject *project, const char *path,
                                         BProjectError *error)
{
    FILE *file = fopen(path, "wb");

    if (file == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                      "Could not create generated CMakeLists.txt.");

    const char *languages = project->languageMode == BPROJECT_LANGUAGE_C     ? "C"
                            : project->languageMode == BPROJECT_LANGUAGE_CPP ? "CXX"
                                                                             : "C CXX";
    const char *mainSource =
        project->languageMode == BPROJECT_LANGUAGE_CPP ? "source/main.cpp" : "source/main.c";
    const char *gameSource =
        project->languageMode == BPROJECT_LANGUAGE_CPP ? "source/game.cpp" : "source/game.c";

    fprintf(file,
            "cmake_minimum_required(VERSION 3.25)\n\n"
            "project(%s LANGUAGES %s)\n\n",
            project->identifier, languages);

    if (project->languageMode != BPROJECT_LANGUAGE_CPP) {
        fprintf(file,
                "set(CMAKE_C_STANDARD %d)\n"
                "set(CMAKE_C_STANDARD_REQUIRED ON)\n"
                "set(CMAKE_C_EXTENSIONS OFF)\n\n",
                project->cStandard);
    }

    if (project->languageMode != BPROJECT_LANGUAGE_C) {
        fprintf(file,
                "set(CMAKE_CXX_STANDARD %d)\n"
                "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
                "set(CMAKE_CXX_EXTENSIONS OFF)\n\n",
                project->cppStandard);
    }

    fprintf(file,
            "set(BASIL_ENGINE_ROOT \"\" CACHE PATH \"Path to the BasilEngine source tree\")\n\n"
            "if(NOT BASIL_ENGINE_ROOT)\n"
            "    message(FATAL_ERROR \"Set BASIL_ENGINE_ROOT to the BasilEngine source tree.\")\n"
            "endif()\n\n"
            "set(BASIL_BUILD_REFERENCE_GAME OFF CACHE BOOL \"\" FORCE)\n"
            "set(BASIL_BUILD_PROJECT_TOOL OFF CACHE BOOL \"\" FORCE)\n"
            "set(BUILD_TESTING OFF CACHE BOOL \"\" FORCE)\n"
            "add_subdirectory(\"${BASIL_ENGINE_ROOT}\" \"${CMAKE_BINARY_DIR}/_basilengine\")\n\n"
            "add_executable(%s %s)\n"
            "add_library(%sGame MODULE %s\n",
            project->identifier, mainSource, project->identifier, gameSource);

    if (project->languageMode == BPROJECT_LANGUAGE_MIXED)
        fprintf(file, "    source/ProjectExtension.cpp\n");

    fprintf(file,
            ")\n"
            "set_target_properties(%sGame PROPERTIES PREFIX \"\" OUTPUT_NAME \"%s.candidate\")\n"
            "target_link_libraries(%s PRIVATE BasilEngine)\n"
            "target_link_libraries(%sGame PRIVATE BasilGameAPI)\n"
            "add_dependencies(%s %sGame)\n"
            "add_custom_command(TARGET %sGame POST_BUILD\n"
            "    COMMAND ${CMAKE_COMMAND} -E copy_if_different\n"
            "        \"$<TARGET_FILE:%sGame>\"\n"
            "        \"$<TARGET_FILE_DIR:%s>/%s.game${CMAKE_SHARED_MODULE_SUFFIX}\"\n"
            ")\n",
            project->identifier, project->identifier, project->identifier, project->identifier,
            project->identifier, project->identifier, project->identifier, project->identifier,
            project->identifier, project->identifier);

    bool succeeded = ferror(file) == 0 && fclose(file) == 0;

    if (!succeeded)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                      "Could not write generated CMakeLists.txt.");

    return true;
}

static bool BProjectGenerator_WriteGame(const BProject *project, const char *path,
                                        BProjectError *error)
{
    FILE *file = fopen(path, "wb");
    if (file == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                      "Could not create generated game module.");
    const char *extension =
        project->languageMode == BPROJECT_LANGUAGE_MIXED ? "#include \"ProjectExtension.h\"\n" : "";
    const char *extensionCall = project->languageMode == BPROJECT_LANGUAGE_MIXED
                                    ? "    host->log(host->context, BasilProject_GetTitle());\n"
                                    : "";
    fprintf(file,
            "#include \"BGameModule.h\"\n"
            "#include <string.h>\n"
            "%s\n"
            "typedef struct GameState\n"
            "{\n"
            "    const BGameHostAPI* host;\n"
            "} GameState;\n\n"
            "static bool Game_Initialize(const BGameHostAPI* host, void** gameState)\n"
            "{\n"
            "    static GameState state = {0};\n"
            "\n"
            "    state.host = host;\n"
            "    *gameState = &state;\n"
            "\n"
            "    host->log(host->context, \"%s game module initialized.\");\n"
            "%s"
            "    return true;\n"
            "}\n\n"
            "static void Game_Update(void* gameState, float deltaTime) { (void)gameState; "
            "(void)deltaTime; }\n"
            "static void Game_Render(void* gameState) { (void)gameState; }\n"
            "static void Game_Shutdown(void* gameState) { (void)gameState; }\n\n"
            "BGAME_MODULE_EXPORT bool BasilGame_Query(uint32_t hostVersion, BGameModule* module)\n"
            "{\n"
            "    if (hostVersion != BGAME_API_VERSION || module == 0) return false;\n"
            "    memset(module, 0, sizeof(*module));\n"
            "    module->version = BGAME_API_VERSION;\n"
            "    module->structSize = sizeof(*module);\n"
            "    module->name = \"%s\";\n"
            "    module->onInitialize = Game_Initialize;\n"
            "    module->onUpdate = Game_Update;\n"
            "    module->onRender = Game_Render;\n"
            "    module->onShutdown = Game_Shutdown;\n"
            "    return true;\n"
            "}\n",
            extension, project->identifier, extensionCall, project->identifier);
    bool succeeded = ferror(file) == 0 && fclose(file) == 0;
    return succeeded ? true
                     : BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                              "Could not write generated game module.");
}

static bool BProjectGenerator_WriteMain(const BProject *project, const char *path,
                                        BProjectError *error)
{
    FILE *file = fopen(path, "wb");

    if (file == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                      "Could not create generated entry point.");

    fprintf(file,
            "#include \"BGeneratedRuntime.h\"\n"
            "\n"
            "int main(int argc, char** argv)\n"
            "{\n"
            "    return BGeneratedRuntime_Run(argc, argv, \"%s\");\n"
            "}\n",
            project->identifier);

    bool succeeded = ferror(file) == 0 && fclose(file) == 0;

    if (!succeeded)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO,
                                      "Could not write generated entry point.");

    return true;
}

bool BProjectGenerator_Create(const BProject *project, const char *parentDirectory,
                              BProjectError *error)
{
    if (error != 0) {
        error->code = BPROJECT_ERROR_NONE;
        error->message[0] = '\0';
    }

    if (parentDirectory == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT,
                                      "Parent directory is required.");

    if (!BProject_Validate(project, error))
        return false;

    if (!BProjectGenerator_PathExists(parentDirectory))
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT,
                                      "Parent directory does not exist.");

    char root[BPROJECT_PATH_MAX];

    if (!BProjectGenerator_Path(root, sizeof(root), parentDirectory, project->identifier, error))
        return false;

    if (BProjectGenerator_PathExists(root))
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_ALREADY_EXISTS,
                                      "Project directory already exists.");

    if (!BProjectGenerator_CreateDirectory(root, error))
        return false;

    const char *directories[] = {".basil", "assets", "workspaces", "source"};

    for (size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i) {
        char directory[BPROJECT_PATH_MAX];

        if (!BProjectGenerator_Path(directory, sizeof(directory), root, directories[i], error) ||
            !BProjectGenerator_CreateDirectory(directory, error)) {
            return false;
        }
    }

    char path[BPROJECT_PATH_MAX];
    char manifestName[BPROJECT_IDENTIFIER_MAX + 16];
    snprintf(manifestName, sizeof(manifestName), "%s.basilproject", project->identifier);

    if (!BProjectGenerator_Path(path, sizeof(path), root, manifestName, error) ||
        !BProject_Save(project, path, error)) {
        return false;
    }

    if (!BProjectGenerator_Path(path, sizeof(path), root, project->startupWorkspace, error))
        return false;

    BWorkspaceDocument workspace;
    BWorkspaceDocument_Init(&workspace);
    BDiagnosticList diagnostics;

    if (!BWorkspaceDocument_CreateDefault(&workspace, "Main Workspace", "Main", &diagnostics) ||
        !BWorkspaceDocument_Save(&workspace, path, &diagnostics)) {
        BWorkspaceDocument_Destroy(&workspace);
        return BProjectGenerator_WorkspaceFailure(error, &diagnostics);
    }

    BWorkspaceDocument_Destroy(&workspace);

    if (!BProjectGenerator_Path(path, sizeof(path), root, ".basil/components.json", error) ||
        !BProjectGenerator_WriteText(path,
                                     "{\n"
                                     "  \"schemaVersion\": 1,\n"
                                     "  \"types\": []\n"
                                     "}\n",
                                     error)) {
        return false;
    }

    if (!BProjectGenerator_Path(path, sizeof(path), root, ".basil/input.json", error) ||
        !BProjectGenerator_WriteText(
            path,
            "{\n  \"schemaVersion\": 1,\n  \"actions\": [\n"
            "    { \"name\": \"move_up\", \"device\": \"keyboard\", \"code\": 87 },\n"
            "    { \"name\": \"move_down\", \"device\": \"keyboard\", \"code\": 83 },\n"
            "    { \"name\": \"move_left\", \"device\": \"keyboard\", \"code\": 65 },\n"
            "    { \"name\": \"move_right\", \"device\": \"keyboard\", \"code\": 68 },\n"
            "    { \"name\": \"confirm\", \"device\": \"keyboard\", \"code\": 257 },\n"
            "    { \"name\": \"cancel\", \"device\": \"keyboard\", \"code\": 256 },\n"
            "    { \"name\": \"primary_action\", \"device\": \"mouse\", \"code\": 0 }\n"
            "  ]\n}\n",
            error))
        return false;

    if (!BProjectGenerator_Path(path, sizeof(path), root, "CMakeLists.txt", error) ||
        !BProjectGenerator_WriteCMake(project, path, error)) {
        return false;
    }

    const char *mainFile =
        project->languageMode == BPROJECT_LANGUAGE_CPP ? "source/main.cpp" : "source/main.c";

    if (!BProjectGenerator_Path(path, sizeof(path), root, mainFile, error) ||
        !BProjectGenerator_WriteMain(project, path, error)) {
        return false;
    }

    const char *gameFile =
        project->languageMode == BPROJECT_LANGUAGE_CPP ? "source/game.cpp" : "source/game.c";
    if (!BProjectGenerator_Path(path, sizeof(path), root, gameFile, error) ||
        !BProjectGenerator_WriteGame(project, path, error))
        return false;

    if (project->languageMode == BPROJECT_LANGUAGE_MIXED) {
        if (!BProjectGenerator_Path(path, sizeof(path), root, "source/ProjectExtension.h", error) ||
            !BProjectGenerator_WriteText(path,
                                         "#ifndef BASIL_PROJECT_EXTENSION_H\n"
                                         "#define BASIL_PROJECT_EXTENSION_H\n\n"
                                         "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n"
                                         "const char* BasilProject_GetTitle(void);\n\n"
                                         "#ifdef __cplusplus\n}\n#endif\n\n"
                                         "#endif\n",
                                         error)) {
            return false;
        }

        if (!BProjectGenerator_Path(path, sizeof(path), root, "source/ProjectExtension.cpp",
                                    error) ||
            !BProjectGenerator_WriteText(path,
                                         "#include \"ProjectExtension.h\"\n\n"
                                         "extern \"C\" const char* BasilProject_GetTitle(void)\n"
                                         "{\n"
                                         "    return \"Empty BasilEngine Project (C + C++)\";\n"
                                         "}\n",
                                         error)) {
            return false;
        }
    }

    if (!BProjectGenerator_Path(path, sizeof(path), root, ".gitignore", error) ||
        !BProjectGenerator_WriteText(path,
                                     "build/\n"
                                     "build-*/\n"
                                     "CMakeUserPresets.json\n"
                                     "*.exe\n*.dll\n*.so\n*.dylib\n*.pdb\n",
                                     error)) {
        return false;
    }

    return true;
}
