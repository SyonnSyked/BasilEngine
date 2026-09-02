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
#include "BWorkspace.h"

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

static BDiagnosticCode FirstErrorCode(const BDiagnosticList* diagnostics)
{
    const BDiagnostic* diagnostic = BDiagnosticList_FirstError(diagnostics);
    return diagnostic != NULL ? diagnostic->code : BDIAGNOSTIC_NONE;
}

int main(void)
{
    int failures = 0;
    BDiagnosticList diagnostics;
    BWorkspaceDocument workspace;
    BWorkspaceDocument_Init(&workspace);

    failures += Check(
        BWorkspaceDocument_CreateDefault(&workspace, "Main Workspace", "Main", &diagnostics),
        "default Workspace is created"
    );
    failures += Check(BWorkspaceDocument_Validate(&workspace, &diagnostics), "default Workspace validates");
    failures += Check(workspace.schemaVersion == BWORKSPACE_SCHEMA_VERSION, "default uses current schema");
    failures += Check(workspace.nextEntityId == 1, "default starts entity IDs at one");

    size_t firstIndex = 0;
    failures += Check(
        BWorkspaceDocument_AddEntity(&workspace, "First Entity", &firstIndex, &diagnostics),
        "entity can be added"
    );
    failures += Check(firstIndex == 0 && workspace.entityCount == 1, "added entity is indexed");
    failures += Check(workspace.entityCapacity >= workspace.entityCount, "entity storage grows on demand");
    failures += Check(strcmp(workspace.entities[0].id, "entity-0000000000000001") == 0, "entity receives stable ID");
    failures += Check(workspace.entities[0].enabled, "new entity is enabled");

    char firstId[BWORKSPACE_ENTITY_ID_MAX];
    snprintf(firstId, sizeof(firstId), "%s", workspace.entities[0].id);
    failures += Check(
        BWorkspaceDocument_RemoveEntity(&workspace, 0, &diagnostics),
        "entity can be removed"
    );
    failures += Check(workspace.entityCount == 0, "remove updates entity count");
    failures += Check(
        BWorkspaceDocument_AddEntity(&workspace, "Replacement", &firstIndex, &diagnostics),
        "replacement can be added"
    );
    failures += Check(strcmp(workspace.entities[0].id, firstId) != 0, "removed entity ID is not reused");

    BWorkspaceDocument collision;
    BWorkspaceDocument_Init(&collision);
    failures += Check(
        BWorkspaceDocument_CreateDefault(&collision, "Collision Workspace", "Collision", &diagnostics),
        "collision Workspace is created"
    );
    size_t collisionIndex = 0;
    failures += Check(
        BWorkspaceDocument_AddEntity(&collision, "Existing", &collisionIndex, &diagnostics),
        "collision fixture entity is added"
    );
    collision.nextEntityId = 1;
    failures += Check(
        BWorkspaceDocument_AddEntity(&collision, "New", &collisionIndex, &diagnostics),
        "ID collision is skipped"
    );
    failures += Check(
        strcmp(collision.entities[collisionIndex].id, "entity-0000000000000002") == 0,
        "collision receives the next available stable ID"
    );

    BWorkspaceDocument invalid;
    BWorkspaceDocument_Init(&invalid);
    failures += Check(
        BWorkspaceDocument_Clone(&workspace, &invalid, &diagnostics),
        "Workspace can be cloned"
    );
    failures += Check(
        invalid.entities != workspace.entities &&
            strcmp(invalid.entities[0].id, workspace.entities[0].id) == 0,
        "clone owns independent entity storage"
    );
    snprintf(invalid.identifier, sizeof(invalid.identifier), "not-valid");
    failures += Check(
        !BWorkspaceDocument_Validate(&invalid, &diagnostics) &&
            FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
        "invalid identifier produces a structured diagnostic"
    );

    char path[BPROJECT_PATH_MAX];
    snprintf(path, sizeof(path), "Workspace_%ld_%d.basilworkspace", (long)time(0), (int)GET_PROCESS_ID());

    failures += Check(BWorkspaceDocument_Save(&workspace, path, &diagnostics), "Workspace saves");
    snprintf(workspace.entities[0].name, sizeof(workspace.entities[0].name), "Renamed Entity");
    failures += Check(BWorkspaceDocument_Save(&workspace, path, &diagnostics), "existing Workspace saves safely");

    char backupPath[BPROJECT_PATH_MAX + 8];
    snprintf(backupPath, sizeof(backupPath), "%s.bak", path);
    failures += Check(FileExists(backupPath), "replacement save preserves a backup");

    BWorkspaceDocument loaded;
    BWorkspaceDocument_Init(&loaded);
    failures += Check(BWorkspaceDocument_Load(path, &loaded, &diagnostics), "saved Workspace loads");
    failures += Check(strcmp(loaded.name, workspace.name) == 0, "loaded name matches");
    failures += Check(strcmp(loaded.identifier, workspace.identifier) == 0, "loaded identifier matches");
    failures += Check(loaded.entityCount == 1, "loaded entity count matches");
    failures += Check(strcmp(loaded.entities[0].name, "Renamed Entity") == 0, "entity edit round trips");
    remove(path);
    remove(backupPath);

    failures += Check(
        BWorkspaceDocument_Load("test-fixtures/workspace-legacy-empty.basilworkspace", &loaded, &diagnostics),
        "legacy empty Workspace loads"
    );
    failures += Check(loaded.schemaVersion == BWORKSPACE_SCHEMA_VERSION, "legacy Workspace upgrades in memory");

    failures += Check(
        BWorkspaceDocument_Load(path, &loaded, &diagnostics) == false &&
            strcmp(loaded.identifier, "LegacyEmpty") == 0,
        "failed load preserves the existing destination"
    );
    failures += Check(
        !BWorkspaceDocument_Load("test-fixtures/workspace-invalid-json.basilworkspace", &loaded, &diagnostics) &&
            FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
        "invalid Workspace JSON is rejected"
    );
    failures += Check(
        !BWorkspaceDocument_Load("test-fixtures/workspace-unsupported-version.basilworkspace", &loaded, &diagnostics) &&
            FirstErrorCode(&diagnostics) == BDIAGNOSTIC_UNSUPPORTED_VERSION,
        "future Workspace schema is rejected"
    );
    failures += Check(
        !BWorkspaceDocument_Load("test-fixtures/workspace-nonempty-entities.basilworkspace", &loaded, &diagnostics) &&
            FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
        "legacy entity content is rejected explicitly"
    );
    failures += Check(
        !BWorkspaceDocument_Load("test-fixtures/workspace-unknown-field.basilworkspace", &loaded, &diagnostics) &&
            FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
        "unknown fields are rejected instead of discarded"
    );
    failures += Check(strcmp(loaded.identifier, "LegacyEmpty") == 0, "all failed loads remain transactional");

    BWorkspaceDocument_Destroy(&loaded);
    BWorkspaceDocument_Destroy(&invalid);
    BWorkspaceDocument_Destroy(&collision);
    BWorkspaceDocument_Destroy(&workspace);
    BWorkspaceDocument_Destroy(&workspace);
    failures += Check(workspace.entities == NULL && workspace.entityCount == 0, "destroy is idempotent");

    if (failures == 0)
        printf("BWorkspaceTests passed.\n");

    return failures == 0 ? 0 : 1;
}
