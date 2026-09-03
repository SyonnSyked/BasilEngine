#ifndef BASIL_ENGINE_PROJECT_CONTEXT_H
#define BASIL_ENGINE_PROJECT_CONTEXT_H

#include "BProject.h"
#include "BDiagnostic.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BProjectContext {
    BProject project;
    char manifestPath[BPROJECT_PATH_MAX];
    char projectRoot[BPROJECT_PATH_MAX];
    char workspacePath[BPROJECT_PATH_MAX];
} BProjectContext;

void BProjectContext_Init(BProjectContext *context);
void BProjectContext_Destroy(BProjectContext *context);
bool BProjectContext_Load(const char *manifestPath, BProjectContext *destination,
                          BDiagnosticList *diagnostics);
bool BProjectContext_ResolvePath(const BProjectContext *context, const char *relativePath,
                                 char *output, size_t outputSize, BDiagnosticList *diagnostics);
bool BProjectContext_Discover(int argumentCount, char **arguments, BProjectContext *destination,
                              BDiagnosticList *diagnostics);

#ifdef __cplusplus
}
#endif

#endif
