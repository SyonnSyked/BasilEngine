#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define GET_PROCESS_ID() _getpid()
#else
#include <unistd.h>
#define GET_PROCESS_ID() getpid()
#endif

#include "BProject.h"
#include "BProjectGenerator.h"

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

static bool FileExists(const char* path)
{
    struct stat information;
    return stat(path, &information) == 0;
}

int main(void)
{
    int failures = 0;
    BProjectError error;
    BProject project = BProject_Default("Test Project", "TestProject");

    failures += Check(BProject_Validate(&project, &error), "default project validates");
    failures += Check(project.languageMode == BPROJECT_LANGUAGE_MIXED, "default project uses mixed languages");
    failures += Check(project.cStandard == 11, "default project uses C11");
    failures += Check(project.cppStandard == 26, "default project uses C++26");
    failures += Check(
        strcmp(project.startupWorkspace, "workspaces/Main.basilworkspace") == 0,
        "default project selects the starter Workspace"
    );

    BProject invalid = project;
    snprintf(invalid.identifier, sizeof(invalid.identifier), "not-valid");
    failures += Check(!BProject_Validate(&invalid, &error), "invalid identifier is rejected");
    failures += Check(error.code == BPROJECT_ERROR_INVALID_MANIFEST, "validation returns a useful error code");

    invalid = project;
    snprintf(invalid.startupWorkspace, sizeof(invalid.startupWorkspace), "../Outside.basilworkspace");
    failures += Check(!BProject_Validate(&invalid, &error), "escaping startup Workspace path is rejected");

    BProject loaded;
    failures += Check(
        !BProject_Load("test-fixtures/project-invalid-json.basilproject", &loaded, &error) &&
            error.code == BPROJECT_ERROR_INVALID_MANIFEST,
        "invalid JSON is rejected"
    );
    failures += Check(
        !BProject_Load("test-fixtures/project-fractional-standard.basilproject", &loaded, &error) &&
            error.code == BPROJECT_ERROR_INVALID_MANIFEST,
        "fractional language standard is rejected"
    );
    failures += Check(
        !BProject_Load("test-fixtures/project-unsupported-version.basilproject", &loaded, &error) &&
            error.code == BPROJECT_ERROR_UNSUPPORTED_VERSION,
        "future schema version is rejected"
    );
    failures += Check(
        BProject_Load("test-fixtures/project-legacy-version.basilproject", &loaded, &error),
        "legacy project manifest loads compatibly"
    );
    failures += Check(
        loaded.schemaVersion == BPROJECT_SCHEMA_VERSION,
        "legacy manifest is represented using the current schema"
    );
    failures += Check(
        strcmp(loaded.startupWorkspace, "scenes/Main.scene") == 0,
        "legacy startup path is preserved until explicit migration"
    );

    char uniqueIdentifier[BPROJECT_IDENTIFIER_MAX];
    snprintf(
        uniqueIdentifier,
        sizeof(uniqueIdentifier),
        "Generated_%ld_%d",
        (long)time(0),
        (int)GET_PROCESS_ID()
    );
    project = BProject_Default("Generated Test Project", uniqueIdentifier);

    failures += Check(BProjectGenerator_Create(&project, ".", &error), "project generator succeeds");

    char root[BPROJECT_PATH_MAX];
    char manifestPath[BPROJECT_PATH_MAX];
    char cmakePath[BPROJECT_PATH_MAX];
    char mainPath[BPROJECT_PATH_MAX];
    char cppPath[BPROJECT_PATH_MAX];
    char workspacePath[BPROJECT_PATH_MAX];
    snprintf(root, sizeof(root), "./%s", uniqueIdentifier);
    snprintf(manifestPath, sizeof(manifestPath), "%s/%s.basilproject", root, uniqueIdentifier);
    snprintf(cmakePath, sizeof(cmakePath), "%s/CMakeLists.txt", root);
    snprintf(mainPath, sizeof(mainPath), "%s/source/main.c", root);
    snprintf(cppPath, sizeof(cppPath), "%s/source/ProjectExtension.cpp", root);
    snprintf(workspacePath, sizeof(workspacePath), "%s/workspaces/Main.basilworkspace", root);

    failures += Check(FileExists(manifestPath), "generator writes manifest");
    failures += Check(FileExists(cmakePath), "generator writes editable CMake file");
    failures += Check(FileExists(mainPath), "generator writes C entry point");
    failures += Check(FileExists(cppPath), "mixed project includes C++ source");
    failures += Check(FileExists(workspacePath), "generator writes starter Workspace");
    failures += Check(
        !BProjectGenerator_Create(&project, ".", &error) &&
            error.code == BPROJECT_ERROR_ALREADY_EXISTS,
        "generator refuses to overwrite an existing project"
    );

    failures += Check(BProject_Load(manifestPath, &loaded, &error), "generated manifest loads");
    failures += Check(strcmp(loaded.name, project.name) == 0, "loaded name matches");
    failures += Check(strcmp(loaded.identifier, project.identifier) == 0, "loaded identifier matches");
    failures += Check(loaded.languageMode == project.languageMode, "loaded language mode matches");
    failures += Check(loaded.cStandard == project.cStandard, "loaded C standard matches");
    failures += Check(loaded.cppStandard == project.cppStandard, "loaded C++ standard matches");
    failures += Check(
        strcmp(loaded.startupWorkspace, project.startupWorkspace) == 0,
        "loaded startup Workspace matches"
    );

    if (failures == 0)
        printf("BProjectTests passed.\n");

    return failures == 0 ? 0 : 1;
}
