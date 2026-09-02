#ifndef BASIL_ENGINE_WORKSPACE_H
#define BASIL_ENGINE_WORKSPACE_H

#include "BDiagnostic.h"

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

typedef struct BWorkspaceDocument
{
    int schemaVersion;
    char name[BWORKSPACE_NAME_MAX];
    char identifier[BWORKSPACE_IDENTIFIER_MAX];
    unsigned long long nextEntityId;
    size_t entityCount;
    size_t entityCapacity;
    BWorkspaceEntity* entities;
} BWorkspaceDocument;

/*
 * Documents own their entity storage. Initialize before first use, destroy when
 * finished, and never copy the structure by assignment or memcpy. Load, Clone,
 * and CreateDefault replace an initialized destination only after success.
 */
void BWorkspaceDocument_Init(BWorkspaceDocument* document);
void BWorkspaceDocument_Destroy(BWorkspaceDocument* document);
void BWorkspaceDocument_Swap(BWorkspaceDocument* left, BWorkspaceDocument* right);

bool BWorkspaceDocument_CreateDefault(
    BWorkspaceDocument* document,
    const char* name,
    const char* identifier,
    BDiagnosticList* diagnostics
);
bool BWorkspaceDocument_Clone(
    const BWorkspaceDocument* source,
    BWorkspaceDocument* destination,
    BDiagnosticList* diagnostics
);
bool BWorkspaceDocument_Validate(
    const BWorkspaceDocument* document,
    BDiagnosticList* diagnostics
);
bool BWorkspaceDocument_Load(
    const char* workspacePath,
    BWorkspaceDocument* destination,
    BDiagnosticList* diagnostics
);
bool BWorkspaceDocument_Save(
    const BWorkspaceDocument* document,
    const char* workspacePath,
    BDiagnosticList* diagnostics
);
bool BWorkspaceDocument_AddEntity(
    BWorkspaceDocument* document,
    const char* name,
    size_t* outIndex,
    BDiagnosticList* diagnostics
);
bool BWorkspaceDocument_RemoveEntity(
    BWorkspaceDocument* document,
    size_t index,
    BDiagnosticList* diagnostics
);

#ifdef __cplusplus
}
#endif

#endif
