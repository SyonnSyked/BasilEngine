#ifndef BASIL_ENGINE_WORKSPACE_H
#define BASIL_ENGINE_WORKSPACE_H

#include "BProject.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BWORKSPACE_SCHEMA_VERSION 1
#define BWORKSPACE_NAME_MAX 128
#define BWORKSPACE_IDENTIFIER_MAX 64

typedef struct BWorkspace
{
    int schemaVersion;
    char name[BWORKSPACE_NAME_MAX];
    char identifier[BWORKSPACE_IDENTIFIER_MAX];
} BWorkspace;

BWorkspace BWorkspace_Default(const char* name, const char* identifier);

bool BWorkspace_Validate(const BWorkspace* workspace, BProjectError* error);
bool BWorkspace_Load(const char* workspacePath, BWorkspace* outWorkspace, BProjectError* error);
bool BWorkspace_Save(const BWorkspace* workspace, const char* workspacePath, BProjectError* error);

#ifdef __cplusplus
}
#endif

#endif
