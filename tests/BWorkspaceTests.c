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

int main(void)
{
    int failures = 0;
    BProjectError error;
    BWorkspace workspace = BWorkspace_Default("Main Workspace", "Main");

    failures += Check(BWorkspace_Validate(&workspace, &error), "default Workspace validates");
    failures += Check(workspace.schemaVersion == BWORKSPACE_SCHEMA_VERSION, "default uses current schema");
    failures += Check(workspace.nextEntityId == 1, "default starts entity IDs at one");

    size_t firstIndex = 0;
    failures += Check(BWorkspace_AddEntity(&workspace, "First Entity", &firstIndex, &error), "entity can be added");
    failures += Check(firstIndex == 0 && workspace.entityCount == 1, "added entity is indexed");
    failures += Check(strcmp(workspace.entities[0].id, "entity-0000000000000001") == 0, "entity receives stable ID");
    failures += Check(workspace.entities[0].enabled, "new entity is enabled");

    char firstId[BWORKSPACE_ENTITY_ID_MAX];
    snprintf(firstId, sizeof(firstId), "%s", workspace.entities[0].id);
    failures += Check(BWorkspace_RemoveEntity(&workspace, 0, &error), "entity can be removed");
    failures += Check(workspace.entityCount == 0, "remove updates entity count");
    failures += Check(BWorkspace_AddEntity(&workspace, "Replacement", &firstIndex, &error), "replacement can be added");
    failures += Check(strcmp(workspace.entities[0].id, firstId) != 0, "removed entity ID is not reused");

    BWorkspace collision = BWorkspace_Default("Collision Workspace", "Collision");
    collision.entityCount = 1;
    collision.nextEntityId = 1;
    snprintf(collision.entities[0].id, sizeof(collision.entities[0].id), "entity-0000000000000001");
    snprintf(collision.entities[0].name, sizeof(collision.entities[0].name), "Existing");
    collision.entities[0].enabled = true;
    size_t collisionIndex = 0;
    failures += Check(BWorkspace_AddEntity(&collision, "New", &collisionIndex, &error), "ID collision is skipped");
    failures += Check(
        strcmp(collision.entities[collisionIndex].id, "entity-0000000000000002") == 0,
        "collision receives the next available stable ID"
    );

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

    snprintf(workspace.entities[0].name, sizeof(workspace.entities[0].name), "Renamed Entity");
    failures += Check(BWorkspace_Save(&workspace, path, &error), "existing Workspace saves safely");

    char backupPath[BPROJECT_PATH_MAX + 8];
    snprintf(backupPath, sizeof(backupPath), "%s.bak", path);
    failures += Check(FileExists(backupPath), "replacement save preserves a backup");

    BWorkspace loaded;
    failures += Check(BWorkspace_Load(path, &loaded, &error), "saved Workspace loads");
    failures += Check(strcmp(loaded.name, workspace.name) == 0, "loaded name matches");
    failures += Check(strcmp(loaded.identifier, workspace.identifier) == 0, "loaded identifier matches");
    failures += Check(loaded.entityCount == 1, "loaded entity count matches");
    failures += Check(strcmp(loaded.entities[0].name, "Renamed Entity") == 0, "entity edit round trips");
    remove(path);
    remove(backupPath);

    failures += Check(
        BWorkspace_Load("test-fixtures/workspace-legacy-empty.basilworkspace", &loaded, &error),
        "legacy empty Workspace loads"
    );
    failures += Check(loaded.schemaVersion == BWORKSPACE_SCHEMA_VERSION, "legacy Workspace upgrades in memory");

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
        "legacy entity content is rejected explicitly"
    );
    failures += Check(
        !BWorkspace_Load("test-fixtures/workspace-unknown-field.basilworkspace", &loaded, &error) &&
            error.code == BPROJECT_ERROR_INVALID_MANIFEST,
        "unknown fields are rejected instead of discarded"
    );

    if (failures == 0)
        printf("BWorkspaceTests passed.\n");

    return failures == 0 ? 0 : 1;
}
