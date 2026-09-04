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

static int Check(bool condition, const char *message)
{
    if (condition)
        return 0;

    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

static bool FileExists(const char *path)
{
    struct stat information;
    return stat(path, &information) == 0;
}

static bool CopyFileContents(const char *sourcePath, const char *destinationPath)
{
    FILE *source = fopen(sourcePath, "rb");
    FILE *destination = fopen(destinationPath, "wb");

    if (source == NULL || destination == NULL) {
        if (source != NULL)
            fclose(source);
        if (destination != NULL)
            fclose(destination);
        return false;
    }

    char buffer[4096];
    size_t count = 0;
    bool succeeded = true;

    while ((count = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, count, destination) != count) {
            succeeded = false;
            break;
        }
    }

    succeeded = succeeded && !ferror(source) && fclose(source) == 0 && fclose(destination) == 0;
    return succeeded;
}

static BDiagnosticCode FirstErrorCode(const BDiagnosticList *diagnostics)
{
    const BDiagnostic *diagnostic = BDiagnosticList_FirstError(diagnostics);
    return diagnostic != NULL ? diagnostic->code : BDIAGNOSTIC_NONE;
}

int main(void)
{
    int failures = 0;
    BDiagnosticList diagnostics;
    BWorkspaceDocument workspace;
    BWorkspaceDocument_Init(&workspace);

    failures +=
        Check(BWorkspaceDocument_CreateDefault(&workspace, "Main Workspace", "Main", &diagnostics),
              "default Workspace is created");
    failures +=
        Check(BWorkspaceDocument_Validate(&workspace, &diagnostics), "default Workspace validates");
    failures +=
        Check(workspace.schemaVersion == BWORKSPACE_SCHEMA_VERSION, "default uses current schema");
    failures += Check(workspace.nextEntityId == 1, "default starts entity IDs at one");

    size_t firstIndex = 0;
    failures +=
        Check(BWorkspaceDocument_AddEntity(&workspace, "First Entity", &firstIndex, &diagnostics),
              "entity can be added");
    failures += Check(firstIndex == 0 && workspace.entityCount == 1, "added entity is indexed");
    failures +=
        Check(workspace.entityCapacity >= workspace.entityCount, "entity storage grows on demand");
    failures += Check(strcmp(workspace.entities[0].id, "entity-0000000000000001") == 0,
                      "entity receives stable ID");
    failures += Check(workspace.entities[0].enabled, "new entity is enabled");

    char firstId[BWORKSPACE_ENTITY_ID_MAX];
    snprintf(firstId, sizeof(firstId), "%s", workspace.entities[0].id);
    failures += Check(BWorkspaceDocument_RemoveEntity(&workspace, 0, &diagnostics),
                      "entity can be removed");
    failures += Check(workspace.entityCount == 0, "remove updates entity count");
    failures +=
        Check(BWorkspaceDocument_AddEntity(&workspace, "Replacement", &firstIndex, &diagnostics),
              "replacement can be added");
    failures +=
        Check(strcmp(workspace.entities[0].id, firstId) != 0, "removed entity ID is not reused");

    BWorkspaceDocument collision;
    BWorkspaceDocument_Init(&collision);
    failures += Check(BWorkspaceDocument_CreateDefault(&collision, "Collision Workspace",
                                                       "Collision", &diagnostics),
                      "collision Workspace is created");
    size_t collisionIndex = 0;
    failures +=
        Check(BWorkspaceDocument_AddEntity(&collision, "Existing", &collisionIndex, &diagnostics),
              "collision fixture entity is added");
    collision.nextEntityId = 1;
    failures +=
        Check(BWorkspaceDocument_AddEntity(&collision, "New", &collisionIndex, &diagnostics),
              "ID collision is skipped");
    failures += Check(strcmp(collision.entities[collisionIndex].id, "entity-0000000000000002") == 0,
                      "collision receives the next available stable ID");

    BWorkspaceDocument invalid;
    BWorkspaceDocument_Init(&invalid);
    failures += Check(BWorkspaceDocument_Clone(&workspace, &invalid, &diagnostics),
                      "Workspace can be cloned");
    failures += Check(invalid.entities != workspace.entities &&
                          strcmp(invalid.entities[0].id, workspace.entities[0].id) == 0,
                      "clone owns independent entity storage");
    snprintf(invalid.identifier, sizeof(invalid.identifier), "not-valid");
    failures += Check(!BWorkspaceDocument_Validate(&invalid, &diagnostics) &&
                          FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
                      "invalid identifier produces a structured diagnostic");

    char path[BPROJECT_PATH_MAX];
    snprintf(path, sizeof(path), "Workspace_%ld_%d.basilworkspace", (long)time(0),
             (int)GET_PROCESS_ID());

    failures += Check(BWorkspaceDocument_Save(&workspace, path, &diagnostics), "Workspace saves");
    snprintf(workspace.entities[0].name, sizeof(workspace.entities[0].name), "Renamed Entity");
    failures += Check(BWorkspaceDocument_Save(&workspace, path, &diagnostics),
                      "existing Workspace saves safely");

    char backupPath[BPROJECT_PATH_MAX + 8];
    snprintf(backupPath, sizeof(backupPath), "%s.bak", path);
    failures += Check(FileExists(backupPath), "replacement save preserves a backup");

    BWorkspaceDocument loaded;
    BWorkspaceDocument_Init(&loaded);
    failures +=
        Check(BWorkspaceDocument_Load(path, &loaded, &diagnostics), "saved Workspace loads");
    failures += Check(strcmp(loaded.name, workspace.name) == 0, "loaded name matches");
    failures +=
        Check(strcmp(loaded.identifier, workspace.identifier) == 0, "loaded identifier matches");
    failures += Check(loaded.entityCount == 1, "loaded entity count matches");
    failures +=
        Check(strcmp(loaded.entities[0].name, "Renamed Entity") == 0, "entity edit round trips");
    remove(path);
    remove(backupPath);

    failures += Check(BWorkspaceDocument_Load("test-fixtures/workspace-legacy-empty.basilworkspace",
                                              &loaded, &diagnostics),
                      "legacy empty Workspace loads");
    failures += Check(loaded.schemaVersion == BWORKSPACE_SCHEMA_VERSION,
                      "legacy Workspace upgrades in memory");

    failures += Check(BWorkspaceDocument_Load(path, &loaded, &diagnostics) == false &&
                          strcmp(loaded.identifier, "LegacyEmpty") == 0,
                      "failed load preserves the existing destination");
    failures +=
        Check(!BWorkspaceDocument_Load("test-fixtures/workspace-invalid-json.basilworkspace",
                                       &loaded, &diagnostics) &&
                  FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
              "invalid Workspace JSON is rejected");
    failures +=
        Check(!BWorkspaceDocument_Load("test-fixtures/workspace-unsupported-version.basilworkspace",
                                       &loaded, &diagnostics) &&
                  FirstErrorCode(&diagnostics) == BDIAGNOSTIC_UNSUPPORTED_VERSION,
              "future Workspace schema is rejected");
    failures +=
        Check(!BWorkspaceDocument_Load("test-fixtures/workspace-nonempty-entities.basilworkspace",
                                       &loaded, &diagnostics) &&
                  FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
              "legacy entity content is rejected explicitly");
    failures +=
        Check(!BWorkspaceDocument_Load("test-fixtures/workspace-unknown-field.basilworkspace",
                                       &loaded, &diagnostics) &&
                  FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
              "unknown fields are rejected instead of discarded");
    failures += Check(strcmp(loaded.identifier, "LegacyEmpty") == 0,
                      "all failed loads remain transactional");

    BWorkspaceDocument components;
    BWorkspaceDocument_Init(&components);
    failures +=
        Check(BWorkspaceDocument_Load("test-fixtures/workspace-components-v3.basilworkspace",
                                      &components, &diagnostics),
              "schema 3 components load");
    failures += Check(components.entityCount == 1 && components.componentCount == 3,
                      "component totals load");
    failures +=
        Check(diagnostics.count == 1 && diagnostics.items[0].severity == BDIAGNOSTIC_WARNING &&
                  strcmp(diagnostics.items[0].entityId, "entity-0000000000000001") == 0 &&
                  strcmp(diagnostics.items[0].componentType, "example.gameplay-note") == 0,
              "unknown optional component produces a structured warning");
    const BWorkspaceEntity *rendered = &components.entities[0];
    const BWorkspaceComponent *transform =
        BWorkspaceEntity_FindComponentConst(rendered, BWORKSPACE_TRANSFORM2D_TYPE);
    const BWorkspaceComponent *ascii =
        BWorkspaceEntity_FindComponentConst(rendered, BWORKSPACE_ASCII_RENDERABLE_TYPE);
    const BWorkspaceComponent *unknown =
        BWorkspaceEntity_FindComponentConst(rendered, "example.gameplay-note");
    failures +=
        Check(transform != NULL && transform->kind == BWORKSPACE_COMPONENT_TRANSFORM2D &&
                  transform->data.transform2d.x == 12.5f && transform->data.transform2d.y == -3.25f,
              "Transform2D decodes into typed data");
    failures += Check(
        ascii != NULL && ascii->kind == BWORKSPACE_COMPONENT_ASCII_RENDERABLE &&
            ascii->data.asciiRenderable.sourceKind == BASCII_SOURCE_TEXT_SPRITE &&
            strcmp(ascii->data.asciiRenderable.textSprite.path, "assets/sprites/player.txt") == 0 &&
            ascii->data.asciiRenderable.anchor == BASCII_ANCHOR_CENTER &&
            ascii->data.asciiRenderable.layer == 7,
        "ASCII Renderable decodes into typed data");
    failures += Check(unknown != NULL && unknown->kind == BWORKSPACE_COMPONENT_UNKNOWN &&
                          strstr(unknown->data.unknownDataJson, "preserve me") != NULL,
                      "unknown optional component is preserved");

    BWorkspaceDocument componentClone;
    BWorkspaceDocument_Init(&componentClone);
    failures += Check(BWorkspaceDocument_Clone(&components, &componentClone, &diagnostics),
                      "component Workspace deep clones");
    const BWorkspaceComponent *clonedUnknown =
        BWorkspaceEntity_FindComponentConst(&componentClone.entities[0], "example.gameplay-note");
    failures +=
        Check(clonedUnknown != NULL &&
                  clonedUnknown->data.unknownDataJson != unknown->data.unknownDataJson &&
                  strcmp(clonedUnknown->data.unknownDataJson, unknown->data.unknownDataJson) == 0,
              "unknown component payload has independent ownership");
    size_t duplicateIndex = 0;
    failures +=
        Check(BWorkspaceDocument_DuplicateEntity(&components, 0, &duplicateIndex, &diagnostics),
              "entity with optional component duplicates");
    unknown = BWorkspaceEntity_FindComponentConst(&components.entities[0], "example.gameplay-note");
    const BWorkspaceComponent *duplicatedUnknown = BWorkspaceEntity_FindComponentConst(
        &components.entities[duplicateIndex], "example.gameplay-note");
    failures += Check(
        duplicateIndex == 1 && duplicatedUnknown != NULL &&
            duplicatedUnknown->data.unknownDataJson != unknown->data.unknownDataJson &&
            strcmp(duplicatedUnknown->data.unknownDataJson, unknown->data.unknownDataJson) == 0,
        "duplicated optional component has independent preserved data");

    char componentPath[BPROJECT_PATH_MAX];
    snprintf(componentPath, sizeof(componentPath), "Components_%ld_%d.basilworkspace",
             (long)time(0), (int)GET_PROCESS_ID());
    failures += Check(BWorkspaceDocument_Save(&components, componentPath, &diagnostics),
                      "component Workspace saves");
    BWorkspaceDocument roundTrip;
    BWorkspaceDocument_Init(&roundTrip);
    failures += Check(BWorkspaceDocument_Load(componentPath, &roundTrip, &diagnostics),
                      "component Workspace reloads");
    const BWorkspaceComponent *roundTripUnknown =
        BWorkspaceEntity_FindComponentConst(&roundTrip.entities[0], "example.gameplay-note");
    failures += Check(roundTripUnknown != NULL &&
                          strstr(roundTripUnknown->data.unknownDataJson, "preserve me") != NULL,
                      "unknown optional data survives load-save-load");
    remove(componentPath);

    failures +=
        Check(!BWorkspaceDocument_Load("test-fixtures/workspace-required-unknown-v3.basilworkspace",
                                       &roundTrip, &diagnostics) &&
                  FirstErrorCode(&diagnostics) == BDIAGNOSTIC_UNSUPPORTED_VERSION &&
                  strcmp(roundTrip.identifier, "Components") == 0,
              "unknown required component is rejected transactionally");
    failures += Check(
        !BWorkspaceDocument_Load("test-fixtures/workspace-duplicate-components-v3.basilworkspace",
                                 &roundTrip, &diagnostics) &&
            FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
        "duplicate component types are rejected");

    BWorkspaceDocument previous;
    BWorkspaceDocument_Init(&previous);
    failures += Check(BWorkspaceDocument_Load("test-fixtures/workspace-entities-v2.basilworkspace",
                                              &previous, &diagnostics),
                      "schema 2 entity Workspace loads");
    failures += Check(previous.schemaVersion == BWORKSPACE_SCHEMA_VERSION &&
                          previous.entityCount == 1 && previous.entities[0].componentCount == 0,
                      "schema 2 entities migrate in memory without invented components");
    failures += Check(BWorkspaceDocument_RequiresMigration(&previous),
                      "schema 2 source records pending migration");

    char migrationPath[BPROJECT_PATH_MAX];
    snprintf(migrationPath, sizeof(migrationPath), "Migration_%ld_%d.basilworkspace", (long)time(0),
             (int)GET_PROCESS_ID());
    failures +=
        Check(CopyFileContents("test-fixtures/workspace-entities-v2.basilworkspace", migrationPath),
              "migration fixture is copied");
    failures += Check(BWorkspaceDocument_Save(&previous, migrationPath, &diagnostics),
                      "schema 2 document saves as schema 3");
    char migrationBackup[BPROJECT_PATH_MAX + 8];
    snprintf(migrationBackup, sizeof(migrationBackup), "%s.bak", migrationPath);
    failures += Check(FileExists(migrationBackup), "schema migration retains the original backup");
    BWorkspaceDocument migrated;
    BWorkspaceDocument_Init(&migrated);
    failures += Check(BWorkspaceDocument_Load(migrationPath, &migrated, &diagnostics),
                      "saved schema 3 migration reloads");
    remove(migrationPath);
    remove(migrationBackup);

    BWorkspaceDocument mutations;
    BWorkspaceDocument_Init(&mutations);
    failures += Check(BWorkspaceDocument_CreateDefault(&mutations, "Mutation Workspace",
                                                       "Mutations", &diagnostics),
                      "mutation Workspace is created");
    size_t mutationEntity = 0;
    failures +=
        Check(BWorkspaceDocument_AddEntity(&mutations, "Actor", &mutationEntity, &diagnostics),
              "mutation entity is added");
    failures +=
        Check(BWorkspaceDocument_AddTransform2D(&mutations, mutationEntity,
                                                (BTransform2D){1.5f, 2.5f}, true, &diagnostics),
              "Transform2D can be added");
    BAsciiRenderable glyph = BAsciiRenderable_DefaultGlyph('@');
    failures += Check(BWorkspaceDocument_AddAsciiRenderable(&mutations, mutationEntity, &glyph,
                                                            true, &diagnostics),
                      "ASCII Renderable can be added");
    failures +=
        Check(!BWorkspaceDocument_AddTransform2D(&mutations, mutationEntity,
                                                 (BTransform2D){0.0f, 0.0f}, true, &diagnostics),
              "duplicate component mutation is rejected");
    failures +=
        Check(BWorkspaceDocument_RemoveComponent(&mutations, mutationEntity,
                                                 BWORKSPACE_ASCII_RENDERABLE_TYPE, &diagnostics) &&
                  mutations.componentCount == 1,
              "component can be removed with totals repaired");

    BWorkspaceDocument componentsV4;
    BWorkspaceDocument_Init(&componentsV4);

    failures +=
        Check(BWorkspaceDocument_Load("test-fixtures/workspace-components-v4.basilworkspace",
                                      &componentsV4, &diagnostics),
              "schema 4 components load");

    const BWorkspaceEntity *renderedV4 = &componentsV4.entities[0];

    const BWorkspaceComponent *asciiV4 =
        BWorkspaceEntity_FindComponentConst(renderedV4, BWORKSPACE_ASCII_RENDERABLE_TYPE);

    failures += Check(
        asciiV4 != NULL && asciiV4->kind == BWORKSPACE_COMPONENT_ASCII_RENDERABLE &&
            asciiV4->data.asciiRenderable.sourceKind == BASCII_SOURCE_TEXT_SPRITE &&
            strcmp(asciiV4->data.asciiRenderable.textSprite.id, "asset-player-test") == 0 &&
            strcmp(asciiV4->data.asciiRenderable.textSprite.path, "assets/sprites/player.txt") == 0,
        "schema 4 Text Sprite loads stable ID and path");

    BWorkspaceDocument_Destroy(&mutations);
    BWorkspaceDocument_Destroy(&migrated);
    BWorkspaceDocument_Destroy(&previous);
    BWorkspaceDocument_Destroy(&roundTrip);
    BWorkspaceDocument_Destroy(&componentClone);
    BWorkspaceDocument_Destroy(&components);

    BWorkspaceDocument_Destroy(&loaded);
    BWorkspaceDocument_Destroy(&invalid);
    BWorkspaceDocument_Destroy(&collision);
    BWorkspaceDocument_Destroy(&workspace);
    BWorkspaceDocument_Destroy(&workspace);
    BWorkspaceDocument_Destroy(&componentsV4);
    failures +=
        Check(workspace.entities == NULL && workspace.entityCount == 0, "destroy is idempotent");

    if (failures == 0)
        printf("BWorkspaceTests passed.\n");

    return failures == 0 ? 0 : 1;
}
