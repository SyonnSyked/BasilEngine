#include "BWorkspaceReplacement.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define MAKE_DIRECTORY(path) _mkdir(path)
#define REMOVE_DIRECTORY(path) _rmdir(path)
#define GET_PROCESS_ID() _getpid()
#define FULL_PATH(output, path) _fullpath(output, path, BPROJECT_PATH_MAX)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIRECTORY(path) mkdir(path, 0755)
#define REMOVE_DIRECTORY(path) rmdir(path)
#define GET_PROCESS_ID() getpid()
#define FULL_PATH(output, path) realpath(path, output)
#endif

static int Check(bool condition, const char *message)
{
    if (condition)
        return 0;
    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

static bool WriteText(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL)
        return false;
    size_t length = strlen(text);
    bool result = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && result;
}

static bool CreateWorkspace(const char *path, const char *name, size_t entityCount)
{
    BDiagnosticList diagnostics = {0};
    BWorkspaceDocument document;
    BWorkspaceDocument_Init(&document);
    bool result = BWorkspaceDocument_CreateDefault(&document, name, name, &diagnostics);
    for (size_t i = 0; result && i < entityCount; ++i) {
        char entityName[32];
        snprintf(entityName, sizeof(entityName), "%s Entity %zu", name, i + 1);
        result = BWorkspaceDocument_AddEntity(&document, entityName, NULL, &diagnostics);
    }
    result = result && BWorkspaceDocument_Save(&document, path, &diagnostics);
    BWorkspaceDocument_Destroy(&document);
    return result;
}

int main(void)
{
    int failures = 0;
    char temporary[256];
    char workspaces[320];
    char pathA[384];
    char pathB[384];
    char malformedPath[384];
    snprintf(temporary, sizeof(temporary), "Workspace Replacement Test %ld %d",
             (long)time(NULL), (int)GET_PROCESS_ID());
    snprintf(workspaces, sizeof(workspaces), "%s/workspaces", temporary);
    snprintf(pathA, sizeof(pathA), "%s/A.basilworkspace", workspaces);
    snprintf(pathB, sizeof(pathB), "%s/B.basilworkspace", workspaces);
    snprintf(malformedPath, sizeof(malformedPath), "%s/Malformed.basilworkspace", workspaces);
    failures += Check(MAKE_DIRECTORY(temporary) == 0, "temporary Project root is created");
    failures += Check(MAKE_DIRECTORY(workspaces) == 0, "Workspace directory is created");
    failures += Check(CreateWorkspace(pathA, "A", 1), "Workspace A is created");
    failures += Check(CreateWorkspace(pathB, "B", 2), "Workspace B is created");
    failures += Check(WriteText(malformedPath, "{ not valid json"),
                      "malformed Workspace is created");

    BProjectContext context;
    BProjectContext_Init(&context);
    failures += Check(FULL_PATH(context.projectRoot, temporary) != NULL,
                      "Project root is resolved");
    failures += Check(FULL_PATH(context.workspacePath, pathA) != NULL,
                      "initial Workspace path is resolved");

    BDiagnosticList diagnostics = {0};
    BWorkspaceDocument active;
    BWorkspaceDocument_Init(&active);
    failures += Check(BWorkspaceDocument_Load(context.workspacePath, &active, &diagnostics),
                      "Workspace A loads");
    BTextSpriteCache cache;
    BTextSpriteCache_Init(&cache);
    BAsciiDrawList drawList;
    BAsciiDrawList_Init(&drawList);
    failures += Check(BAsciiDrawList_Build(&active, context.projectRoot, &cache, &drawList,
                                           &diagnostics),
                      "Workspace A runtime interpretation builds");

    BWorkspaceReplacementRequest request = {0};
    uint32_t generation = 1;
    char message[BDIAGNOSTIC_MESSAGE_MAX];

    const BWorkspaceEntity *callbackEntity = &active.entities[0];
    failures += Check(BWorkspaceReplacement_Request(&request, "workspaces/B.basilworkspace"),
                      "callback can queue Workspace B");
    failures += Check(active.entityCount == 1 && strcmp(callbackEntity->name, "A Entity 1") == 0,
                      "request does not mutate Workspace during callback");
    failures += Check(BWorkspaceReplacement_Process(&request, &context, &active, &cache,
                                                    &drawList, &generation, message,
                                                    sizeof(message)) ==
                          BWORKSPACE_REPLACEMENT_SUCCEEDED,
                      "Workspace B replaces A at the boundary");
    failures += Check(active.entityCount == 2 && strcmp(active.entities[1].name, "B Entity 2") == 0,
                      "runtime-visible content comes from Workspace B");
    failures += Check(generation == 2 && strstr(context.workspacePath, "B.basilworkspace") != NULL,
                      "successful replacement advances runtime identity");

    failures += Check(BWorkspaceReplacement_Request(&request, "workspaces/Missing.basilworkspace"),
                      "missing Workspace request queues");
    failures += Check(BWorkspaceReplacement_Process(&request, &context, &active, &cache,
                                                    &drawList, &generation, message,
                                                    sizeof(message)) ==
                          BWORKSPACE_REPLACEMENT_FAILED,
                      "missing Workspace fails");
    failures += Check(active.entityCount == 2 && generation == 2 &&
                          strstr(message, "Missing.basilworkspace") != NULL &&
                          strstr(message, "retained") != NULL,
                      "missing failure names request and retains B");

    failures += Check(BWorkspaceReplacement_Request(&request,
                                                    "workspaces/Malformed.basilworkspace"),
                      "malformed Workspace request queues");
    failures += Check(BWorkspaceReplacement_Process(&request, &context, &active, &cache,
                                                    &drawList, &generation, message,
                                                    sizeof(message)) ==
                          BWORKSPACE_REPLACEMENT_FAILED,
                      "malformed Workspace fails");
    failures += Check(active.entityCount == 2 && generation == 2 &&
                          strcmp(active.entities[0].name, "B Entity 1") == 0,
                      "malformed replacement leaves B fully usable");

    failures += Check(BWorkspaceReplacement_Request(&request, "workspaces/A.basilworkspace"),
                      "valid request queues after failures");
    failures += Check(BWorkspaceReplacement_Process(&request, &context, &active, &cache,
                                                    &drawList, &generation, message,
                                                    sizeof(message)) ==
                          BWORKSPACE_REPLACEMENT_SUCCEEDED,
                      "valid replacement succeeds after failures");
    failures += Check(active.entityCount == 1 && generation == 3 &&
                          strcmp(active.entities[0].name, "A Entity 1") == 0,
                      "B to A replacement is runtime-visible");

    BAsciiDrawList_Destroy(&drawList);
    BTextSpriteCache_Destroy(&cache);
    BWorkspaceDocument_Destroy(&active);
    remove(malformedPath);
    remove(pathB);
    remove(pathA);
    failures += Check(REMOVE_DIRECTORY(workspaces) == 0, "Workspace directory is removed");
    failures += Check(REMOVE_DIRECTORY(temporary) == 0, "temporary Project root is removed");
    if (failures == 0)
        printf("BWorkspaceReplacementTests passed.\n");
    return failures == 0 ? 0 : 1;
}
