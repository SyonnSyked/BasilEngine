#ifndef BASIL_ENGINE_WORKSPACE_REPLACEMENT_H
#define BASIL_ENGINE_WORKSPACE_REPLACEMENT_H

#include "BAsciiDrawList.h"
#include "BProjectContext.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BWorkspaceReplacementRequest {
    bool pending;
    char path[BPROJECT_PATH_MAX];
} BWorkspaceReplacementRequest;

typedef enum BWorkspaceReplacementResult {
    BWORKSPACE_REPLACEMENT_NONE,
    BWORKSPACE_REPLACEMENT_SUCCEEDED,
    BWORKSPACE_REPLACEMENT_FAILED
} BWorkspaceReplacementResult;

bool BWorkspaceReplacement_Request(BWorkspaceReplacementRequest *request,
                                   const char *workspacePath);
BWorkspaceReplacementResult BWorkspaceReplacement_Process(
    BWorkspaceReplacementRequest *request, BProjectContext *context,
    BWorkspaceDocument *document, BTextSpriteCache *cache, BAsciiDrawList *drawList,
    uint32_t *generation, char *message, size_t messageSize);

#ifdef __cplusplus
}
#endif

#endif
