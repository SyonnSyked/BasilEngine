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

static bool BProjectGenerator_Fail(
    BProjectError* error,
    BProjectErrorCode code,
    const char* message
)
{
    if (error != 0)
    {
        error->code = code;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }

    return false;
}

static bool BProjectGenerator_WorkspaceFailure(
    BProjectError* error,
    const BDiagnosticList* diagnostics
)
{
    const BDiagnostic* diagnostic = BDiagnosticList_FirstError(diagnostics);
    BProjectErrorCode code = BPROJECT_ERROR_INVALID_MANIFEST;

    if (diagnostic != 0)
    {
        if (diagnostic->code == BDIAGNOSTIC_IO || diagnostic->code == BDIAGNOSTIC_OUT_OF_MEMORY)
            code = BPROJECT_ERROR_IO;
        else if (diagnostic->code == BDIAGNOSTIC_INVALID_ARGUMENT)
            code = BPROJECT_ERROR_INVALID_ARGUMENT;
        else if (diagnostic->code == BDIAGNOSTIC_UNSUPPORTED_VERSION)
            code = BPROJECT_ERROR_UNSUPPORTED_VERSION;
    }

    return BProjectGenerator_Fail(
        error,
        code,
        diagnostic != 0 ? diagnostic->message : "Workspace generation failed."
    );
}

static bool BProjectGenerator_Path(
    char* output,
    size_t outputSize,
    const char* left,
    const char* right,
    BProjectError* error
)
{
    int written = snprintf(output, outputSize, "%s/%s", left, right);

    if (written < 0 || (size_t)written >= outputSize)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Generated project path is too long.");

    return true;
}

static bool BProjectGenerator_PathExists(const char* path)
{
    struct stat information;
    return stat(path, &information) == 0;
}

static bool BProjectGenerator_CreateDirectory(const char* path, BProjectError* error)
{
    if (BPROJECT_MKDIR(path) == 0)
        return true;

    if (errno == EEXIST)
        return true;

    return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO, "Could not create a project directory.");
}

static bool BProjectGenerator_WriteText(
    const char* path,
    const char* contents,
    BProjectError* error
)
{
    FILE* file = fopen(path, "wb");

    if (file == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO, "Could not create a generated project file.");

    size_t length = strlen(contents);
    bool succeeded = fwrite(contents, 1, length, file) == length;
    fclose(file);

    if (!succeeded)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO, "Could not write a complete generated project file.");

    return true;
}

static bool BProjectGenerator_WriteCMake(
    const BProject* project,
    const char* path,
    BProjectError* error
)
{
    FILE* file = fopen(path, "wb");

    if (file == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO, "Could not create generated CMakeLists.txt.");

    const char* languages = project->languageMode == BPROJECT_LANGUAGE_C ? "C" :
        project->languageMode == BPROJECT_LANGUAGE_CPP ? "CXX" : "C CXX";
    const char* mainSource = project->languageMode == BPROJECT_LANGUAGE_CPP ?
        "source/main.cpp" : "source/main.c";

    fprintf(file,
        "cmake_minimum_required(VERSION 3.25)\n\n"
        "project(%s LANGUAGES %s)\n\n",
        project->identifier,
        languages
    );

    if (project->languageMode != BPROJECT_LANGUAGE_CPP)
    {
        fprintf(file,
            "set(CMAKE_C_STANDARD %d)\n"
            "set(CMAKE_C_STANDARD_REQUIRED ON)\n"
            "set(CMAKE_C_EXTENSIONS OFF)\n\n",
            project->cStandard
        );
    }

    if (project->languageMode != BPROJECT_LANGUAGE_C)
    {
        fprintf(file,
            "set(CMAKE_CXX_STANDARD %d)\n"
            "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
            "set(CMAKE_CXX_EXTENSIONS OFF)\n\n",
            project->cppStandard
        );
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
        "add_executable(%s\n"
        "    %s\n",
        project->identifier,
        mainSource
    );

    if (project->languageMode == BPROJECT_LANGUAGE_MIXED)
        fprintf(file, "    source/ProjectExtension.cpp\n");

    fprintf(file,
        ")\n\n"
        "target_link_libraries(%s PRIVATE BasilEngine)\n",
        project->identifier
    );

    bool succeeded = ferror(file) == 0 && fclose(file) == 0;

    if (!succeeded)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO, "Could not write generated CMakeLists.txt.");

    return true;
}

static bool BProjectGenerator_WriteMain(
    const BProject* project,
    const char* path,
    BProjectError* error
)
{
    const char* extensionDeclaration = project->languageMode == BPROJECT_LANGUAGE_MIXED ?
        "#include \"ProjectExtension.h\"\n" : "";
    const char* titleExpression = project->languageMode == BPROJECT_LANGUAGE_MIXED ?
        "BasilProject_GetTitle()" : "\"Empty BasilEngine Project\"";
    FILE* file = fopen(path, "wb");

    if (file == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO, "Could not create generated entry point.");

    fprintf(file,
        "#include <stdbool.h>\n\n"
        "#include <raylib.h>\n\n"
        "#include \"BApplication.h\"\n"
        "%s\n"
        "static bool Project_OnStart(void* userData, BEngine* engine)\n"
        "{\n"
        "    (void)userData;\n"
        "    (void)engine;\n"
        "    return true;\n"
        "}\n\n"
        "static void Project_OnUpdate(void* userData, BEngine* engine, float deltaTime)\n"
        "{\n"
        "    (void)userData;\n"
        "    (void)engine;\n"
        "    (void)deltaTime;\n"
        "}\n\n"
        "static void Project_OnRender(void* userData, BEngine* engine)\n"
        "{\n"
        "    (void)userData;\n"
        "    (void)engine;\n"
        "    ClearBackground(BLACK);\n"
        "}\n\n"
        "static void Project_OnShutdown(void* userData, BEngine* engine)\n"
        "{\n"
        "    (void)userData;\n"
        "    (void)engine;\n"
        "}\n\n"
        "int main(void)\n"
        "{\n"
        "    BEngineConfig config = BEngineConfig_Default();\n"
        "    config.windowConfig.title = %s;\n\n"
        "    BApplicationCallbacks callbacks = { 0 };\n"
        "    callbacks.onStart = Project_OnStart;\n"
        "    callbacks.onUpdate = Project_OnUpdate;\n"
        "    callbacks.onRender = Project_OnRender;\n"
        "    callbacks.onShutdown = Project_OnShutdown;\n\n"
        "    BApplication application;\n\n"
        "    if (!BApplication_Init(&application, config, callbacks))\n"
        "        return 1;\n\n"
        "    return BApplication_Run(&application);\n"
        "}\n",
        extensionDeclaration,
        titleExpression
    );

    bool succeeded = ferror(file) == 0 && fclose(file) == 0;

    if (!succeeded)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_IO, "Could not write generated entry point.");

    return true;
}

bool BProjectGenerator_Create(
    const BProject* project,
    const char* parentDirectory,
    BProjectError* error
)
{
    if (error != 0)
    {
        error->code = BPROJECT_ERROR_NONE;
        error->message[0] = '\0';
    }

    if (parentDirectory == 0)
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Parent directory is required.");

    if (!BProject_Validate(project, error))
        return false;

    if (!BProjectGenerator_PathExists(parentDirectory))
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Parent directory does not exist.");

    char root[BPROJECT_PATH_MAX];

    if (!BProjectGenerator_Path(root, sizeof(root), parentDirectory, project->identifier, error))
        return false;

    if (BProjectGenerator_PathExists(root))
        return BProjectGenerator_Fail(error, BPROJECT_ERROR_ALREADY_EXISTS, "Project directory already exists.");

    if (!BProjectGenerator_CreateDirectory(root, error))
        return false;

    const char* directories[] = { "assets", "workspaces", "source" };

    for (size_t i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i)
    {
        char directory[BPROJECT_PATH_MAX];

        if (!BProjectGenerator_Path(directory, sizeof(directory), root, directories[i], error) ||
            !BProjectGenerator_CreateDirectory(directory, error))
        {
            return false;
        }
    }

    char path[BPROJECT_PATH_MAX];
    char manifestName[BPROJECT_IDENTIFIER_MAX + 16];
    snprintf(manifestName, sizeof(manifestName), "%s.basilproject", project->identifier);

    if (!BProjectGenerator_Path(path, sizeof(path), root, manifestName, error) ||
        !BProject_Save(project, path, error))
    {
        return false;
    }

    if (!BProjectGenerator_Path(path, sizeof(path), root, project->startupWorkspace, error))
        return false;

    BWorkspaceDocument workspace;
    BWorkspaceDocument_Init(&workspace);
    BDiagnosticList diagnostics;

    if (!BWorkspaceDocument_CreateDefault(
        &workspace,
        "Main Workspace",
        "Main",
        &diagnostics
    ) || !BWorkspaceDocument_Save(&workspace, path, &diagnostics))
    {
        BWorkspaceDocument_Destroy(&workspace);
        return BProjectGenerator_WorkspaceFailure(error, &diagnostics);
    }

    BWorkspaceDocument_Destroy(&workspace);

    if (!BProjectGenerator_Path(path, sizeof(path), root, "CMakeLists.txt", error) ||
        !BProjectGenerator_WriteCMake(project, path, error))
    {
        return false;
    }

    const char* mainFile = project->languageMode == BPROJECT_LANGUAGE_CPP ?
        "source/main.cpp" : "source/main.c";

    if (!BProjectGenerator_Path(path, sizeof(path), root, mainFile, error) ||
        !BProjectGenerator_WriteMain(project, path, error))
    {
        return false;
    }

    if (project->languageMode == BPROJECT_LANGUAGE_MIXED)
    {
        if (!BProjectGenerator_Path(path, sizeof(path), root, "source/ProjectExtension.h", error) ||
            !BProjectGenerator_WriteText(path,
                "#ifndef BASIL_PROJECT_EXTENSION_H\n"
                "#define BASIL_PROJECT_EXTENSION_H\n\n"
                "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n"
                "const char* BasilProject_GetTitle(void);\n\n"
                "#ifdef __cplusplus\n}\n#endif\n\n"
                "#endif\n",
                error))
        {
            return false;
        }

        if (!BProjectGenerator_Path(path, sizeof(path), root, "source/ProjectExtension.cpp", error) ||
            !BProjectGenerator_WriteText(path,
                "#include \"ProjectExtension.h\"\n\n"
                "extern \"C\" const char* BasilProject_GetTitle(void)\n"
                "{\n"
                "    return \"Empty BasilEngine Project (C + C++)\";\n"
                "}\n",
                error))
        {
            return false;
        }
    }

    if (!BProjectGenerator_Path(path, sizeof(path), root, ".gitignore", error) ||
        !BProjectGenerator_WriteText(path,
            "build/\n"
            "build-*/\n"
            "CMakeUserPresets.json\n"
            "*.exe\n*.dll\n*.so\n*.dylib\n*.pdb\n",
            error))
    {
        return false;
    }

    return true;
}
