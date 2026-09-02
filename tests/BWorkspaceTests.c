#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define GET_PROCESS_ID() _getpid()
#else
#include <unistd.h>
#define GET_PROCESS_ID() getpid()
#endif

#include "BWorkspace.h"

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

int main(void)
{
    int failures = 0;
    BProjectError error;
    BWorkspace workspace = BWorkspace_Default("Main Workspace", "Main");

    failures += Check(BWorkspace_Validate(&workspace, &error), "default Workspace validates");
    failures += Check(workspace.schemaVersion == BWORKSPACE_SCHEMA_VERSION, "default uses current schema");

    BWorkspace invalid = workspace;
    snprintf(invalid.identifier, sizeof(invalid.identifier), "not-valid");
    failures += Check(!BWorkspace_Validate(&invalid, &error), "invalid identifier is rejected");

    char path[BPROJECT_PATH_MAX];
    snprintf(
        path,
        sizeof(path),
        "Workspace_%ld_%d.basilworkspace",
        (long)time(0),
        (int)GET_PROCESS_ID()
    );

    failures += Check(BWorkspace_Save(&workspace, path, &error), "Workspace saves");

    BWorkspace loaded;
    failures += Check(BWorkspace_Load(path, &loaded, &error), "saved Workspace loads");
    failures += Check(strcmp(loaded.name, workspace.name) == 0, "loaded name matches");
    failures += Check(strcmp(loaded.identifier, workspace.identifier) == 0, "loaded identifier matches");
    remove(path);

    failures += Check(
        !BWorkspace_Load("test-fixtures/workspace-invalid-json.basilworkspace", &loaded, &error) &&
            error.code == BPROJECT_ERROR_INVALID_MANIFEST,
        "invalid Workspace JSON is rejected"
    );
    failures += Check(
        !BWorkspace_Load("test-fixtures/workspace-unsupported-version.basilworkspace", &loaded, &error) &&
            error.code == BPROJECT_ERROR_UNSUPPORTED_VERSION,
        "future Workspace schema is rejected"
    );
    failures += Check(
        !BWorkspace_Load("test-fixtures/workspace-nonempty-entities.basilworkspace", &loaded, &error) &&
            error.code == BPROJECT_ERROR_INVALID_MANIFEST,
        "unimplemented entity content is rejected explicitly"
    );

    if (failures == 0)
        printf("BWorkspaceTests passed.\n");

    return failures == 0 ? 0 : 1;
}
