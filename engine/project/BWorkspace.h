#ifndef BASIL_ENGINE_WORKSPACE_H
#define BASIL_ENGINE_WORKSPACE_H

#include "BProject.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BWORKSPACE_SCHEMA_VERSION 2
#define BWORKSPACE_LEGACY_SCHEMA_VERSION 1
#define BWORKSPACE_NAME_MAX 128
#define BWORKSPACE_IDENTIFIER_MAX 64
#define BWORKSPACE_ENTITY_MAX 512
#define BWORKSPACE_ENTITY_ID_MAX 32
#define BWORKSPACE_ENTITY_NAME_MAX 128

typedef struct BWorkspaceEntity
{
    char id[BWORKSPACE_ENTITY_ID_MAX];
    char name[BWORKSPACE_ENTITY_NAME_MAX];
    bool enabled;
} BWorkspaceEntity;

typedef struct BWorkspace
{
    int schemaVersion;
    char name[BWORKSPACE_NAME_MAX];
    char identifier[BWORKSPACE_IDENTIFIER_MAX];
    unsigned long long nextEntityId;
    size_t entityCount;
    BWorkspaceEntity entities[BWORKSPACE_ENTITY_MAX];
} BWorkspace;

BWorkspace BWorkspace_Default(const char* name, const char* identifier);

bool BWorkspace_Validate(const BWorkspace* workspace, BProjectError* error);
bool BWorkspace_Load(const char* workspacePath, BWorkspace* outWorkspace, BProjectError* error);
bool BWorkspace_Save(const BWorkspace* workspace, const char* workspacePath, BProjectError* error);
bool BWorkspace_AddEntity(
    BWorkspace* workspace,
    const char* name,
    size_t* outIndex,
    BProjectError* error
);
bool BWorkspace_RemoveEntity(BWorkspace* workspace, size_t index, BProjectError* error);

#ifdef __cplusplus
}
#endif

#endif
