#include "BWorkspaceReplacement.h"

#include <stdio.h>
#include <string.h>

static void SetMessage(char *message, size_t messageSize, const char *format,
                       const char *path, const char *detail)
{
    if (message == NULL || messageSize == 0)
        return;
    snprintf(message, messageSize, format, path, detail);
}

bool BWorkspaceReplacement_Request(BWorkspaceReplacementRequest *request,
                                   const char *workspacePath)
{
    if (request == NULL || workspacePath == NULL || workspacePath[0] == '\0' ||
        request->pending)
        return false;
    size_t length = strlen(workspacePath);
    if (length >= sizeof(request->path))
        return false;
    memcpy(request->path, workspacePath, length + 1);
    request->pending = true;
    return true;
}

BWorkspaceReplacementResult BWorkspaceReplacement_Process(
    BWorkspaceReplacementRequest *request, BProjectContext *context,
    BWorkspaceDocument *document, BTextSpriteCache *cache, BAsciiDrawList *drawList,
    uint32_t *generation, char *message, size_t messageSize)
{
    if (request == NULL || !request->pending)
        return BWORKSPACE_REPLACEMENT_NONE;

    char requested[BPROJECT_PATH_MAX];
    snprintf(requested, sizeof(requested), "%s", request->path);
    request->pending = false;
    request->path[0] = '\0';

    if (context == NULL || document == NULL || cache == NULL || drawList == NULL ||
        generation == NULL) {
        SetMessage(message, messageSize,
                   "Workspace replacement '%s' failed: invalid runtime state; current Workspace retained.",
                   requested, "");
        return BWORKSPACE_REPLACEMENT_FAILED;
    }
    if (*generation == UINT32_MAX) {
        SetMessage(message, messageSize,
                   "Workspace replacement '%s' failed: generation limit reached; current Workspace retained.",
                   requested, "");
        return BWORKSPACE_REPLACEMENT_FAILED;
    }

    BDiagnosticList diagnostics = {0};
    char resolved[BPROJECT_PATH_MAX];
    if (!BProjectContext_ResolvePath(context, requested, resolved, sizeof(resolved),
                                     &diagnostics)) {
        const BDiagnostic *error = BDiagnosticList_FirstError(&diagnostics);
        SetMessage(message, messageSize,
                   "Workspace replacement '%s' failed: %s Current Workspace retained.", requested,
                   error != NULL ? error->message : "path resolution failed.");
        return BWORKSPACE_REPLACEMENT_FAILED;
    }

    BWorkspaceDocument candidate;
    BWorkspaceDocument_Init(&candidate);
    BAsciiDrawList candidateDrawList;
    BAsciiDrawList_Init(&candidateDrawList);
    bool valid = BWorkspaceDocument_Load(resolved, &candidate, &diagnostics) &&
                 BAsciiDrawList_Build(&candidate, context->projectRoot, cache,
                                      &candidateDrawList, &diagnostics);
    if (!valid) {
        const BDiagnostic *error = BDiagnosticList_FirstError(&diagnostics);
        SetMessage(message, messageSize,
                   "Workspace replacement '%s' failed: %s Current Workspace retained.", requested,
                   error != NULL ? error->message : "load or validation failed.");
        BAsciiDrawList_Destroy(&candidateDrawList);
        BWorkspaceDocument_Destroy(&candidate);
        return BWORKSPACE_REPLACEMENT_FAILED;
    }

    BWorkspaceDocument_Swap(document, &candidate);
    BAsciiDrawList_Swap(drawList, &candidateDrawList);
    snprintf(context->workspacePath, sizeof(context->workspacePath), "%s", resolved);
    ++*generation;
    SetMessage(message, messageSize, "Workspace replacement '%s' completed%s", requested, ".");
    BAsciiDrawList_Destroy(&candidateDrawList);
    BWorkspaceDocument_Destroy(&candidate);
    return BWORKSPACE_REPLACEMENT_SUCCEEDED;
}
