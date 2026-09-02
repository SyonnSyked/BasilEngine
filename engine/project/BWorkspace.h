#ifndef BASIL_ENGINE_WORKSPACE_H
#define BASIL_ENGINE_WORKSPACE_H

#include "BDiagnostic.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BWORKSPACE_SCHEMA_VERSION 3
#define BWORKSPACE_PREVIOUS_SCHEMA_VERSION 2
#define BWORKSPACE_LEGACY_SCHEMA_VERSION 1
#define BWORKSPACE_NAME_MAX 128
#define BWORKSPACE_IDENTIFIER_MAX 64
#define BWORKSPACE_ENTITY_MAX 512
#define BWORKSPACE_ENTITY_ID_MAX 32
#define BWORKSPACE_ENTITY_NAME_MAX 128
#define BWORKSPACE_COMPONENT_MAX 4096
#define BWORKSPACE_ENTITY_COMPONENT_MAX 16
#define BWORKSPACE_COMPONENT_TYPE_MAX 96
#define BWORKSPACE_UNKNOWN_DATA_MAX (64 * 1024)
#define BWORKSPACE_FILE_MAX (4 * 1024 * 1024)
#define BWORKSPACE_PATH_MAX 1024

#define BWORKSPACE_TRANSFORM2D_TYPE "basil.transform2d"
#define BWORKSPACE_ASCII_RENDERABLE_TYPE "basil.ascii-renderable"

typedef enum BWorkspaceComponentKind
{
    BWORKSPACE_COMPONENT_UNKNOWN,
    BWORKSPACE_COMPONENT_TRANSFORM2D,
    BWORKSPACE_COMPONENT_ASCII_RENDERABLE
} BWorkspaceComponentKind;

typedef enum BAsciiSourceKind
{
    BASCII_SOURCE_GLYPH,
    BASCII_SOURCE_TEXT_SPRITE
} BAsciiSourceKind;

typedef enum BAsciiAnchor
{
    BASCII_ANCHOR_BOTTOM_CENTER,
    BASCII_ANCHOR_CENTER,
    BASCII_ANCHOR_TOP_LEFT
} BAsciiAnchor;

typedef struct BTransform2D
{
    float x;
    float y;
} BTransform2D;

typedef struct BAsciiColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} BAsciiColor;

typedef struct BAsciiRenderable
{
    BAsciiSourceKind sourceKind;
    char glyph;
    char textSpritePath[BWORKSPACE_PATH_MAX];
    BAsciiColor foreground;
    BAsciiColor background;
    short layer;
    BAsciiAnchor anchor;
    bool visible;
    bool transparentSpaces;
} BAsciiRenderable;

typedef struct BWorkspaceComponent
{
    char type[BWORKSPACE_COMPONENT_TYPE_MAX];
    int version;
    bool required;
    BWorkspaceComponentKind kind;
    union
    {
        BTransform2D transform2d;
        BAsciiRenderable asciiRenderable;
        char* unknownDataJson;
    } data;
} BWorkspaceComponent;

typedef struct BWorkspaceEntity
{
    char id[BWORKSPACE_ENTITY_ID_MAX];
    char name[BWORKSPACE_ENTITY_NAME_MAX];
    bool enabled;
    size_t componentCount;
    size_t componentCapacity;
    BWorkspaceComponent* components;
} BWorkspaceEntity;

typedef struct BWorkspaceDocument
{
    int schemaVersion;
    int sourceSchemaVersion;
    char name[BWORKSPACE_NAME_MAX];
    char identifier[BWORKSPACE_IDENTIFIER_MAX];
    unsigned long long nextEntityId;
    size_t entityCount;
    size_t entityCapacity;
    size_t componentCount;
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
bool BWorkspaceDocument_AddTransform2D(
    BWorkspaceDocument* document,
    size_t entityIndex,
    BTransform2D transform,
    bool required,
    BDiagnosticList* diagnostics
);
bool BWorkspaceDocument_AddAsciiRenderable(
    BWorkspaceDocument* document,
    size_t entityIndex,
    const BAsciiRenderable* renderable,
    bool required,
    BDiagnosticList* diagnostics
);
bool BWorkspaceDocument_RemoveComponent(
    BWorkspaceDocument* document,
    size_t entityIndex,
    const char* type,
    BDiagnosticList* diagnostics
);
BWorkspaceComponent* BWorkspaceEntity_FindComponent(BWorkspaceEntity* entity, const char* type);
const BWorkspaceComponent* BWorkspaceEntity_FindComponentConst(
    const BWorkspaceEntity* entity,
    const char* type
);
BAsciiRenderable BAsciiRenderable_DefaultGlyph(char glyph);
bool BWorkspaceDocument_RequiresMigration(const BWorkspaceDocument* document);

#ifdef __cplusplus
}
#endif

#endif
