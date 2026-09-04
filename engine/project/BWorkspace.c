#include "BWorkspace.h"

#include "cJSON.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void BWorkspaceDocument_ClearError(BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);
}

static bool BWorkspaceDocument_Fail(BDiagnosticList *diagnostics, BDiagnosticCode code,
                                    const char *message)
{
    BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, code, message, 0);
    return false;
}

static bool BWorkspaceDocument_Reserve(BWorkspaceDocument *document, size_t required,
                                       BDiagnosticList *diagnostics)
{
    if (required <= document->entityCapacity)
        return true;

    if (required > BWORKSPACE_ENTITY_MAX)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace entity capacity has been reached.");

    size_t capacity = document->entityCapacity == 0 ? 8 : document->entityCapacity;

    while (capacity < required) {
        if (capacity >= BWORKSPACE_ENTITY_MAX / 2) {
            capacity = BWORKSPACE_ENTITY_MAX;
            break;
        }

        capacity *= 2;
    }

    BWorkspaceEntity *entities =
        (BWorkspaceEntity *)realloc(document->entities, capacity * sizeof(*entities));

    if (entities == 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                       "Out of memory while growing the Workspace entity list.");

    memset(entities + document->entityCapacity, 0,
           (capacity - document->entityCapacity) * sizeof(*entities));
    document->entities = entities;
    document->entityCapacity = capacity;
    return true;
}

static void BWorkspaceComponent_Destroy(BWorkspaceComponent *component)
{
    if (component == 0)
        return;

    if (component->kind == BWORKSPACE_COMPONENT_UNKNOWN)
        free(component->data.unknownDataJson);

    memset(component, 0, sizeof(*component));
}

static void BWorkspaceEntity_Destroy(BWorkspaceEntity *entity)
{
    if (entity == 0)
        return;

    for (size_t i = 0; i < entity->componentCount; ++i)
        BWorkspaceComponent_Destroy(&entity->components[i]);

    free(entity->components);
    memset(entity, 0, sizeof(*entity));
}

static bool BWorkspaceEntity_ReserveComponents(BWorkspaceEntity *entity, size_t required,
                                               BDiagnosticList *diagnostics)
{
    if (required <= entity->componentCapacity)
        return true;

    if (required > BWORKSPACE_ENTITY_COMPONENT_MAX)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Entity component capacity has been reached.");

    size_t capacity = entity->componentCapacity == 0 ? 4 : entity->componentCapacity * 2;

    if (capacity < required)
        capacity = required;
    if (capacity > BWORKSPACE_ENTITY_COMPONENT_MAX)
        capacity = BWORKSPACE_ENTITY_COMPONENT_MAX;

    BWorkspaceComponent *components =
        (BWorkspaceComponent *)realloc(entity->components, capacity * sizeof(*components));

    if (components == 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                       "Out of memory while growing an entity component list.");

    memset(components + entity->componentCapacity, 0,
           (capacity - entity->componentCapacity) * sizeof(*components));
    entity->components = components;
    entity->componentCapacity = capacity;
    return true;
}

static bool BWorkspace_IsValidComponentType(const char *type)
{
    if (type == 0 || type[0] == '\0' || strlen(type) >= BWORKSPACE_COMPONENT_TYPE_MAX)
        return false;

    bool hasNamespace = false;

    for (size_t i = 0; type[i] != '\0'; ++i) {
        unsigned char value = (unsigned char)type[i];

        if (value == '.')
            hasNamespace = i > 0 && type[i + 1] != '\0';
        else if (!(islower(value) || isdigit(value) || value == '-' || value == '_'))
            return false;
    }

    return hasNamespace;
}

static bool BWorkspace_IsRelativeAssetPath(const char *path)
{
    if (path == 0 || path[0] == '\0' || strlen(path) >= BWORKSPACE_PATH_MAX || path[0] == '/' ||
        path[0] == '\\' || (isalpha((unsigned char)path[0]) && path[1] == ':')) {
        return false;
    }

    const char *segment = path;

    for (const char *cursor = path;; ++cursor) {
        if (*cursor == '\\')
            return false;

        if (*cursor == '/' || *cursor == '\0') {
            size_t length = (size_t)(cursor - segment);

            if (length == 0 || (length == 1 && segment[0] == '.') ||
                (length == 2 && segment[0] == '.' && segment[1] == '.')) {
                return false;
            }

            if (*cursor == '\0')
                break;

            segment = cursor + 1;
        }
    }

    return true;
}

static bool BWorkspace_IsRenderableValidForSchema(const BAsciiRenderable *renderable,
                                                  int sourceSchemaVersion)
{
    if (renderable == NULL || renderable->anchor < BASCII_ANCHOR_BOTTOM_CENTER ||
        renderable->anchor > BASCII_ANCHOR_TOP_LEFT ||
        renderable->sourceKind < BASCII_SOURCE_GLYPH ||
        renderable->sourceKind > BASCII_SOURCE_TEXT_SPRITE) {
        return false;
    }

    if (renderable->sourceKind == BASCII_SOURCE_GLYPH) {
        return (unsigned char)renderable->glyph >= 0x20 && (unsigned char)renderable->glyph <= 0x7e;
    }

    if (sourceSchemaVersion >= BWORKSPACE_ASSET_REF_SCHEMA_VERSION) {
        BDiagnosticList diagnostics = {0};

        return BAssetRef_Validate(&renderable->textSprite, &diagnostics);
    }

    return renderable->textSprite.id[0] == '\0' &&
           BWorkspace_IsRelativeAssetPath(renderable->textSprite.path);
}

static bool BWorkspaceDocument_IsValidIdentifier(const char *identifier)
{
    if (identifier == 0 || identifier[0] == '\0')
        return false;

    if (!(isalpha((unsigned char)identifier[0]) || identifier[0] == '_'))
        return false;

    for (size_t i = 1; identifier[i] != '\0'; ++i) {
        if (!(isalnum((unsigned char)identifier[i]) || identifier[i] == '_'))
            return false;
    }

    return true;
}

static bool BWorkspaceDocument_HasOnlyFields(const cJSON *object, const char *const *fields,
                                             size_t fieldCount)
{
    if (!cJSON_IsObject(object))
        return false;

    for (const cJSON *item = object->child; item != 0; item = item->next) {
        bool known = false;

        for (const cJSON *previous = object->child; previous != item; previous = previous->next) {
            if (strcmp(previous->string, item->string) == 0)
                return false;
        }

        for (size_t i = 0; i < fieldCount; ++i) {
            if (strcmp(item->string, fields[i]) == 0) {
                known = true;
                break;
            }
        }

        if (!known)
            return false;
    }

    return true;
}

static char *BWorkspaceDocument_ReadFile(const char *path, BDiagnosticList *error)
{
    FILE *file = fopen(path, "rb");

    if (file == 0) {
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not open the Workspace file.");
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not measure the Workspace file.");
        return 0;
    }

    long length = ftell(file);

    if (length < 0 || (uintmax_t)length > SIZE_MAX - 1 || (uintmax_t)length > BWORKSPACE_FILE_MAX ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not read the Workspace file.");
        return 0;
    }

    size_t fileLength = (size_t)length;
    char *contents = (char *)calloc(fileLength + 1, 1);

    if (contents == 0) {
        fclose(file);
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO,
                                "Out of memory while reading the Workspace.");
        return 0;
    }

    size_t bytesRead = fread(contents, 1, fileLength, file);
    fclose(file);

    if (bytesRead != fileLength) {
        free(contents);
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO,
                                "Could not read the complete Workspace file.");
        return 0;
    }

    return contents;
}

void BWorkspaceDocument_Init(BWorkspaceDocument *document)
{
    if (document != 0)
        memset(document, 0, sizeof(*document));
}

void BWorkspaceDocument_Destroy(BWorkspaceDocument *document)
{
    if (document == 0)
        return;

    for (size_t i = 0; i < document->entityCount; ++i)
        BWorkspaceEntity_Destroy(&document->entities[i]);

    free(document->entities);
    memset(document, 0, sizeof(*document));
}

void BWorkspaceDocument_Swap(BWorkspaceDocument *left, BWorkspaceDocument *right)
{
    if (left == 0 || right == 0 || left == right)
        return;

    BWorkspaceDocument temporary = *left;
    *left = *right;
    *right = temporary;
}

bool BWorkspaceDocument_CreateDefault(BWorkspaceDocument *document, const char *name,
                                      const char *identifier, BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);

    if (document == 0 || name == 0 || identifier == 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace destination, name, and identifier are required.");

    if (strlen(name) >= BWORKSPACE_NAME_MAX || strlen(identifier) >= BWORKSPACE_IDENTIFIER_MAX)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace name or identifier is too long.");

    BWorkspaceDocument workspace;
    BWorkspaceDocument_Init(&workspace);
    workspace.schemaVersion = BWORKSPACE_SCHEMA_VERSION;
    workspace.sourceSchemaVersion = BWORKSPACE_SCHEMA_VERSION;
    workspace.nextEntityId = 1;

    snprintf(workspace.name, sizeof(workspace.name), "%s", name);
    snprintf(workspace.identifier, sizeof(workspace.identifier), "%s", identifier);

    if (!BWorkspaceDocument_Validate(&workspace, diagnostics))
        return false;

    BWorkspaceDocument_Swap(document, &workspace);
    BWorkspaceDocument_Destroy(&workspace);
    return true;
}

bool BWorkspaceDocument_Clone(const BWorkspaceDocument *source, BWorkspaceDocument *destination,
                              BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);

    if (source == 0 || destination == 0 || source == destination)
        return BWorkspaceDocument_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
            "Distinct Workspace source and destination documents are required.");

    if (!BWorkspaceDocument_Validate(source, diagnostics))
        return false;

    BWorkspaceDocument clone;
    BWorkspaceDocument_Init(&clone);
    clone.schemaVersion = source->schemaVersion;
    clone.sourceSchemaVersion = source->sourceSchemaVersion;
    clone.nextEntityId = source->nextEntityId;
    snprintf(clone.name, sizeof(clone.name), "%s", source->name);
    snprintf(clone.identifier, sizeof(clone.identifier), "%s", source->identifier);

    if (!BWorkspaceDocument_Reserve(&clone, source->entityCount, diagnostics)) {
        BWorkspaceDocument_Destroy(&clone);
        return false;
    }

    for (size_t i = 0; i < source->entityCount; ++i) {
        const BWorkspaceEntity *sourceEntity = &source->entities[i];
        BWorkspaceEntity *cloneEntity = &clone.entities[i];
        snprintf(cloneEntity->id, sizeof(cloneEntity->id), "%s", sourceEntity->id);
        snprintf(cloneEntity->name, sizeof(cloneEntity->name), "%s", sourceEntity->name);
        cloneEntity->enabled = sourceEntity->enabled;

        if (!BWorkspaceEntity_ReserveComponents(cloneEntity, sourceEntity->componentCount,
                                                diagnostics)) {
            clone.entityCount = i + 1;
            BWorkspaceDocument_Destroy(&clone);
            return false;
        }

        for (size_t componentIndex = 0; componentIndex < sourceEntity->componentCount;
             ++componentIndex) {
            const BWorkspaceComponent *sourceComponent = &sourceEntity->components[componentIndex];
            BWorkspaceComponent *cloneComponent = &cloneEntity->components[componentIndex];
            *cloneComponent = *sourceComponent;

            if (sourceComponent->kind == BWORKSPACE_COMPONENT_UNKNOWN) {
                cloneComponent->data.unknownDataJson = 0;
                size_t length = strlen(sourceComponent->data.unknownDataJson);
                cloneComponent->data.unknownDataJson = (char *)malloc(length + 1);

                if (cloneComponent->data.unknownDataJson == 0) {
                    cloneEntity->componentCount = componentIndex;
                    clone.entityCount = i + 1;
                    BWorkspaceDocument_Destroy(&clone);
                    return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                                   "Out of memory while cloning component data.");
                }

                memcpy(cloneComponent->data.unknownDataJson, sourceComponent->data.unknownDataJson,
                       length + 1);
            }

            cloneEntity->componentCount += 1;
            clone.componentCount += 1;
        }

        clone.entityCount += 1;
    }
    BWorkspaceDocument_Swap(destination, &clone);
    BWorkspaceDocument_Destroy(&clone);
    return true;
}

bool BWorkspaceDocument_Validate(const BWorkspaceDocument *workspace, BDiagnosticList *error)
{
    BWorkspaceDocument_ClearError(error);

    if (workspace == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT, "Workspace was null.");

    if (workspace->schemaVersion != BWORKSPACE_SCHEMA_VERSION ||
        workspace->sourceSchemaVersion < BWORKSPACE_LEGACY_SCHEMA_VERSION ||
        workspace->sourceSchemaVersion > BWORKSPACE_SCHEMA_VERSION)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_UNSUPPORTED_VERSION,
                                       "Unsupported Workspace schema version.");

    if (workspace->name[0] == '\0')
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace name is required.");

    if (!BWorkspaceDocument_IsValidIdentifier(workspace->identifier))
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace identifier must be a valid identifier.");

    if (workspace->entityCapacity > BWORKSPACE_ENTITY_MAX ||
        (workspace->entityCapacity > 0 && workspace->entities == 0) ||
        (workspace->entityCapacity == 0 && workspace->entities != 0) ||
        workspace->entityCount > BWORKSPACE_ENTITY_MAX ||
        workspace->entityCount > workspace->entityCapacity || workspace->nextEntityId == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace entity state is invalid.");

    size_t componentTotal = 0;

    for (size_t i = 0; i < workspace->entityCount; ++i) {
        const BWorkspaceEntity *entity = &workspace->entities[i];

        if (entity->id[0] == '\0' || entity->name[0] == '\0')
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                           "Workspace entity identity and name are required.");

        for (size_t other = i + 1; other < workspace->entityCount; ++other) {
            if (strcmp(entity->id, workspace->entities[other].id) == 0)
                return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                               "Workspace entity IDs must be unique.");
        }

        if (entity->componentCapacity > BWORKSPACE_ENTITY_COMPONENT_MAX ||
            entity->componentCount > entity->componentCapacity ||
            (entity->componentCapacity > 0 && entity->components == 0) ||
            (entity->componentCapacity == 0 && entity->components != 0)) {
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                           "Workspace entity component state is invalid.");
        }

        componentTotal += entity->componentCount;

        if (componentTotal > BWORKSPACE_COMPONENT_MAX)
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                           "Workspace contains too many components.");

        for (size_t componentIndex = 0; componentIndex < entity->componentCount; ++componentIndex) {
            const BWorkspaceComponent *component = &entity->components[componentIndex];

            if (!BWorkspace_IsValidComponentType(component->type) || component->version <= 0)
                return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                               "Workspace contains an invalid component envelope.");

            for (size_t other = componentIndex + 1; other < entity->componentCount; ++other) {
                if (strcmp(component->type, entity->components[other].type) == 0)
                    return BWorkspaceDocument_Fail(
                        error, BDIAGNOSTIC_INVALID_DATA,
                        "An entity cannot contain duplicate component types.");
            }

            if (component->kind == BWORKSPACE_COMPONENT_TRANSFORM2D) {
                if (strcmp(component->type, BWORKSPACE_TRANSFORM2D_TYPE) != 0 ||
                    component->version != 1 || !isfinite(component->data.transform2d.x) ||
                    !isfinite(component->data.transform2d.y)) {
                    return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                                   "Transform2D component data is invalid.");
                }
            } else if (component->kind == BWORKSPACE_COMPONENT_ASCII_RENDERABLE) {
                const BAsciiRenderable *renderable = &component->data.asciiRenderable;

                if (strcmp(component->type, BWORKSPACE_ASCII_RENDERABLE_TYPE) != 0 ||
                    component->version != 1 || renderable->anchor < BASCII_ANCHOR_BOTTOM_CENTER ||
                    !BWorkspace_IsRenderableValidForSchema(renderable,
                                                           workspace->sourceSchemaVersion)) {
                    return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                                   "ASCII Renderable component data is invalid.");
                }
            } else if (component->kind == BWORKSPACE_COMPONENT_UNKNOWN) {
                if (component->required || component->data.unknownDataJson == 0 ||
                    strlen(component->data.unknownDataJson) > BWORKSPACE_UNKNOWN_DATA_MAX) {
                    return BWorkspaceDocument_Fail(
                        error, BDIAGNOSTIC_INVALID_DATA,
                        "Unknown component data is invalid or required.");
                }
            } else {
                return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                               "Workspace component kind is invalid.");
            }
        }

        if (entity->enabled &&
            BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_ASCII_RENDERABLE_TYPE) != 0 &&
            BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_TRANSFORM2D_TYPE) == 0) {
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                           "An enabled ASCII Renderable requires Transform2D.");
        }
    }

    if (componentTotal != workspace->componentCount)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace component count is inconsistent.");

    return true;
}

bool BWorkspaceDocument_RequiresMigration(const BWorkspaceDocument *document)
{
    return document != 0 && document->sourceSchemaVersion < BWORKSPACE_SCHEMA_VERSION;
}

bool BWorkspaceDocument_AddEntity(BWorkspaceDocument *document, const char *name, size_t *outIndex,
                                  BDiagnosticList *error)
{
    BWorkspaceDocument_ClearError(error);

    if (document == 0 || name == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace and entity name are required.");

    if (name[0] == '\0' || strlen(name) >= BWORKSPACE_ENTITY_NAME_MAX)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                       "Entity name is empty or too long.");

    if (document->entityCount >= BWORKSPACE_ENTITY_MAX || document->nextEntityId == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace entity capacity has been reached.");

    if (!BWorkspaceDocument_Reserve(document, document->entityCount + 1, error))
        return false;

    size_t index = document->entityCount;
    BWorkspaceEntity *entity = &document->entities[index];
    bool unique = false;

    while (!unique) {
        int written =
            snprintf(entity->id, sizeof(entity->id), "entity-%016llx", document->nextEntityId);

        if (written < 0 || (size_t)written >= sizeof(entity->id))
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                           "Could not generate an entity ID.");

        unique = true;

        for (size_t i = 0; i < document->entityCount; ++i) {
            if (strcmp(entity->id, document->entities[i].id) == 0) {
                unique = false;
                break;
            }
        }

        if (!unique) {
            if (document->nextEntityId == ULLONG_MAX)
                return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                               "Workspace entity IDs are exhausted.");

            document->nextEntityId += 1;
        }
    }

    if (document->nextEntityId == ULLONG_MAX)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace entity IDs are exhausted.");

    snprintf(entity->name, sizeof(entity->name), "%s", name);
    entity->enabled = true;
    document->entityCount += 1;
    document->nextEntityId += 1;

    if (outIndex != 0)
        *outIndex = index;

    return true;
}

static bool BWorkspaceDocument_AppendComponent(BWorkspaceDocument *document, size_t entityIndex,
                                               const BWorkspaceComponent *component,
                                               BDiagnosticList *diagnostics);

bool BWorkspaceDocument_RemoveEntity(BWorkspaceDocument *document, size_t index,
                                     BDiagnosticList *error)
{
    BWorkspaceDocument_ClearError(error);

    if (document == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace is required.");

    if (index >= document->entityCount)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Entity index is outside the Workspace.");

    document->componentCount -= document->entities[index].componentCount;
    BWorkspaceEntity_Destroy(&document->entities[index]);

    for (size_t i = index + 1; i < document->entityCount; ++i)
        document->entities[i - 1] = document->entities[i];

    document->entityCount -= 1;
    memset(&document->entities[document->entityCount], 0, sizeof(BWorkspaceEntity));
    return true;
}

bool BWorkspaceDocument_DuplicateEntity(BWorkspaceDocument *document, size_t sourceIndex,
                                        size_t *outIndex, BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);
    if (document == 0 || sourceIndex >= document->entityCount)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace and source entity are required.");

    char name[BWORKSPACE_ENTITY_NAME_MAX];
    int written = snprintf(name, sizeof(name), "%s Copy", document->entities[sourceIndex].name);
    if (written < 0 || (size_t)written >= sizeof(name))
        snprintf(name, sizeof(name), "Entity %llu Copy", document->nextEntityId);

    size_t duplicateIndex = 0;
    if (!BWorkspaceDocument_AddEntity(document, name, &duplicateIndex, diagnostics))
        return false;

    BWorkspaceEntity *source = &document->entities[sourceIndex];
    BWorkspaceEntity *duplicate = &document->entities[duplicateIndex];
    duplicate->enabled = source->enabled;

    for (size_t i = 0; i < source->componentCount; ++i) {
        BWorkspaceComponent component = source->components[i];
        if (component.kind == BWORKSPACE_COMPONENT_UNKNOWN) {
            size_t length = strlen(component.data.unknownDataJson);
            component.data.unknownDataJson = (char *)malloc(length + 1);
            if (component.data.unknownDataJson == 0) {
                BWorkspaceDocument_RemoveEntity(document, duplicateIndex, 0);
                return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                               "Out of memory while duplicating component data.");
            }
            memcpy(component.data.unknownDataJson, source->components[i].data.unknownDataJson,
                   length + 1);
        }
        if (!BWorkspaceDocument_AppendComponent(document, duplicateIndex, &component,
                                                diagnostics)) {
            if (component.kind == BWORKSPACE_COMPONENT_UNKNOWN)
                free(component.data.unknownDataJson);
            BWorkspaceDocument_RemoveEntity(document, duplicateIndex, 0);
            return false;
        }
    }

    if (outIndex != 0)
        *outIndex = duplicateIndex;
    return true;
}

bool BWorkspaceDocument_SetEntityName(BWorkspaceDocument *document, size_t entityIndex,
                                      const char *name, BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);
    if (document == 0 || entityIndex >= document->entityCount || name == 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace, entity, and name are required.");
    if (name[0] == '\0' || strlen(name) >= BWORKSPACE_ENTITY_NAME_MAX)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Entity name is empty or too long.");
    snprintf(document->entities[entityIndex].name, sizeof(document->entities[entityIndex].name),
             "%s", name);
    return true;
}

bool BWorkspaceDocument_SetEntityEnabled(BWorkspaceDocument *document, size_t entityIndex,
                                         bool enabled, BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);
    if (document == 0 || entityIndex >= document->entityCount)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace and entity are required.");
    document->entities[entityIndex].enabled = enabled;
    return true;
}

BWorkspaceComponent *BWorkspaceEntity_FindComponent(BWorkspaceEntity *entity, const char *type)
{
    if (entity == 0 || type == 0)
        return 0;

    for (size_t i = 0; i < entity->componentCount; ++i) {
        if (strcmp(entity->components[i].type, type) == 0)
            return &entity->components[i];
    }

    return 0;
}

const BWorkspaceComponent *BWorkspaceEntity_FindComponentConst(const BWorkspaceEntity *entity,
                                                               const char *type)
{
    return BWorkspaceEntity_FindComponent((BWorkspaceEntity *)entity, type);
}

static bool BWorkspaceDocument_AppendComponent(BWorkspaceDocument *document, size_t entityIndex,
                                               const BWorkspaceComponent *component,
                                               BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);

    if (document == 0 || component == 0 || entityIndex >= document->entityCount)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace, entity, and component are required.");

    BWorkspaceEntity *entity = &document->entities[entityIndex];

    if (BWorkspaceEntity_FindComponent(entity, component->type) != 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Entity already contains this component type.");

    if (document->componentCount >= BWORKSPACE_COMPONENT_MAX ||
        !BWorkspaceEntity_ReserveComponents(entity, entity->componentCount + 1, diagnostics)) {
        return false;
    }

    entity->components[entity->componentCount] = *component;
    entity->componentCount += 1;
    document->componentCount += 1;
    return true;
}

static bool BWorkspaceDocument_IsValidComponentType(const char *type)
{
    if (type == 0 || type[0] == '\0' || strlen(type) >= BWORKSPACE_COMPONENT_TYPE_MAX ||
        !isalpha((unsigned char)type[0]))
        return false;
    for (const char *cursor = type; *cursor; ++cursor) {
        if (!isalnum((unsigned char)*cursor) && *cursor != '.' && *cursor != '-' && *cursor != '_')
            return false;
    }
    return true;
}

static char *BWorkspaceDocument_CopyCustomData(const char *dataJson, BDiagnosticList *diagnostics)
{
    if (dataJson == 0 || strlen(dataJson) > BWORKSPACE_UNKNOWN_DATA_MAX) {
        BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                "Custom component data is missing or too large.");
        return 0;
    }
    cJSON *data = cJSON_Parse(dataJson);
    if (data == 0 || !cJSON_IsObject(data)) {
        cJSON_Delete(data);
        BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                "Custom component data must be one JSON object.");
        return 0;
    }
    char *normalized = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);
    if (normalized == 0) {
        BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                "Could not copy custom component data.");
        return 0;
    }
    size_t length = strlen(normalized);
    char *copy = (char *)malloc(length + 1);
    if (copy != 0)
        memcpy(copy, normalized, length + 1);
    cJSON_free(normalized);
    if (copy == 0)
        BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                "Could not copy custom component data.");
    return copy;
}

bool BWorkspaceDocument_AddCustomComponentJson(BWorkspaceDocument *document, size_t entityIndex,
                                               const char *type, int version, const char *dataJson,
                                               BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);
    if (!BWorkspaceDocument_IsValidComponentType(type) || version < 1)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Custom component type and positive version are required.");
    char *copy = BWorkspaceDocument_CopyCustomData(dataJson, diagnostics);
    if (copy == 0)
        return false;
    BWorkspaceComponent component = {0};
    snprintf(component.type, sizeof(component.type), "%s", type);
    component.version = version;
    component.required = false;
    component.kind = BWORKSPACE_COMPONENT_UNKNOWN;
    component.data.unknownDataJson = copy;
    if (!BWorkspaceDocument_AppendComponent(document, entityIndex, &component, diagnostics)) {
        free(copy);
        return false;
    }
    return true;
}

bool BWorkspaceDocument_SetCustomComponentJson(BWorkspaceDocument *document, size_t entityIndex,
                                               const char *type, const char *dataJson,
                                               BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);
    if (document == 0 || entityIndex >= document->entityCount || type == 0)
        return BWorkspaceDocument_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
            "Workspace, entity, and custom component type are required.");
    BWorkspaceComponent *component =
        BWorkspaceEntity_FindComponent(&document->entities[entityIndex], type);
    if (component == 0 || component->kind != BWORKSPACE_COMPONENT_UNKNOWN || component->required)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Editable custom component was not found.");
    char *copy = BWorkspaceDocument_CopyCustomData(dataJson, diagnostics);
    if (copy == 0)
        return false;
    free(component->data.unknownDataJson);
    component->data.unknownDataJson = copy;
    return true;
}

bool BWorkspaceDocument_AddTransform2D(BWorkspaceDocument *document, size_t entityIndex,
                                       BTransform2D transform, bool required,
                                       BDiagnosticList *diagnostics)
{
    BWorkspaceComponent component = {0};
    snprintf(component.type, sizeof(component.type), "%s", BWORKSPACE_TRANSFORM2D_TYPE);
    component.version = 1;
    component.required = required;
    component.kind = BWORKSPACE_COMPONENT_TRANSFORM2D;
    component.data.transform2d = transform;

    if (!isfinite(transform.x) || !isfinite(transform.y)) {
        BWorkspaceDocument_ClearError(diagnostics);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Transform2D position must be finite.");
    }

    return BWorkspaceDocument_AppendComponent(document, entityIndex, &component, diagnostics);
}

BAsciiRenderable BAsciiRenderable_DefaultGlyph(char glyph)
{
    BAsciiRenderable renderable = {0};
    renderable.sourceKind = BASCII_SOURCE_GLYPH;
    renderable.glyph = glyph;
    renderable.foreground = (BAsciiColor){230, 237, 243, 255};
    renderable.background = (BAsciiColor){0, 0, 0, 0};
    renderable.anchor = BASCII_ANCHOR_BOTTOM_CENTER;
    renderable.visible = true;
    renderable.transparentSpaces = true;
    return renderable;
}

bool BWorkspaceDocument_AddAsciiRenderable(BWorkspaceDocument *document, size_t entityIndex,
                                           const BAsciiRenderable *renderable, bool required,
                                           BDiagnosticList *diagnostics)
{
    if (renderable == 0) {
        BWorkspaceDocument_ClearError(diagnostics);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "ASCII Renderable data is required.");
    }

    if (renderable->anchor < BASCII_ANCHOR_BOTTOM_CENTER ||
        renderable->anchor > BASCII_ANCHOR_TOP_LEFT ||
        renderable->sourceKind < BASCII_SOURCE_GLYPH ||
        renderable->sourceKind > BASCII_SOURCE_TEXT_SPRITE ||
        (renderable->sourceKind == BASCII_SOURCE_GLYPH &&
         ((unsigned char)renderable->glyph < 0x20 || (unsigned char)renderable->glyph > 0x7e)) ||
        (renderable->sourceKind == BASCII_SOURCE_TEXT_SPRITE &&
         !BWorkspace_IsRenderableValidForSchema(renderable, renderable->sourceKind))) {
        BWorkspaceDocument_ClearError(diagnostics);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "ASCII Renderable component data is invalid.");
    }

    if (document == 0 || entityIndex >= document->entityCount ||
        BWorkspaceEntity_FindComponent(&document->entities[entityIndex],
                                       BWORKSPACE_TRANSFORM2D_TYPE) == 0) {
        BWorkspaceDocument_ClearError(diagnostics);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "ASCII Renderable requires an entity with Transform2D.");
    }

    BWorkspaceComponent component = {0};
    snprintf(component.type, sizeof(component.type), "%s", BWORKSPACE_ASCII_RENDERABLE_TYPE);
    component.version = 1;
    component.required = required;
    component.kind = BWORKSPACE_COMPONENT_ASCII_RENDERABLE;
    component.data.asciiRenderable = *renderable;
    return BWorkspaceDocument_AppendComponent(document, entityIndex, &component, diagnostics);
}

bool BWorkspaceDocument_SetTransform2D(BWorkspaceDocument *document, size_t entityIndex,
                                       BTransform2D transform, BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);
    if (document == 0 || entityIndex >= document->entityCount)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace and entity are required.");
    if (!isfinite(transform.x) || !isfinite(transform.y))
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Transform2D position must be finite.");
    BWorkspaceComponent *component = BWorkspaceEntity_FindComponent(
        &document->entities[entityIndex], BWORKSPACE_TRANSFORM2D_TYPE);
    if (component == 0 || component->kind != BWORKSPACE_COMPONENT_TRANSFORM2D)
        return BWorkspaceDocument_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_DATA,
            "Entity does not contain a supported Transform2D component.");
    component->data.transform2d = transform;
    return true;
}

bool BWorkspaceDocument_SetAsciiRenderable(BWorkspaceDocument *document, size_t entityIndex,
                                           const BAsciiRenderable *renderable,
                                           BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);
    if (document == 0 || entityIndex >= document->entityCount || renderable == 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace, entity, and ASCII Renderable are required.");
    BWorkspaceComponent *component = BWorkspaceEntity_FindComponent(
        &document->entities[entityIndex], BWORKSPACE_ASCII_RENDERABLE_TYPE);
    if (component == 0 || component->kind != BWORKSPACE_COMPONENT_ASCII_RENDERABLE)
        return BWorkspaceDocument_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_DATA,
            "Entity does not contain a supported ASCII Renderable component.");
    BAsciiRenderable previous = component->data.asciiRenderable;
    component->data.asciiRenderable = *renderable;
    if (!BWorkspaceDocument_Validate(document, diagnostics)) {
        component->data.asciiRenderable = previous;
        return false;
    }
    return true;
}

bool BWorkspaceDocument_RemoveComponent(BWorkspaceDocument *document, size_t entityIndex,
                                        const char *type, BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);

    if (document == 0 || type == 0 || entityIndex >= document->entityCount)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace, entity, and component type are required.");

    BWorkspaceEntity *entity = &document->entities[entityIndex];

    if (strcmp(type, BWORKSPACE_TRANSFORM2D_TYPE) == 0 && entity->enabled &&
        BWorkspaceEntity_FindComponent(entity, BWORKSPACE_ASCII_RENDERABLE_TYPE) != 0) {
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Remove ASCII Renderable before removing its Transform2D.");
    }

    for (size_t i = 0; i < entity->componentCount; ++i) {
        if (strcmp(entity->components[i].type, type) != 0)
            continue;

        BWorkspaceComponent_Destroy(&entity->components[i]);

        for (size_t next = i + 1; next < entity->componentCount; ++next)
            entity->components[next - 1] = entity->components[next];

        entity->componentCount -= 1;
        document->componentCount -= 1;
        memset(&entity->components[entity->componentCount], 0, sizeof(*entity->components));
        return true;
    }

    return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                   "Entity does not contain the requested component.");
}

static int BWorkspace_HexDigit(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

static bool BWorkspace_ParseColor(const cJSON *item, BAsciiColor *color)
{
    if (!cJSON_IsString(item) || strlen(item->valuestring) != 9 || item->valuestring[0] != '#')
        return false;

    unsigned char values[4];

    for (size_t i = 0; i < 4; ++i) {
        int high = BWorkspace_HexDigit(item->valuestring[1 + i * 2]);
        int low = BWorkspace_HexDigit(item->valuestring[2 + i * 2]);

        if (high < 0 || low < 0)
            return false;

        values[i] = (unsigned char)(high * 16 + low);
    }

    color->r = values[0];
    color->g = values[1];
    color->b = values[2];
    color->a = values[3];
    return true;
}

static bool BWorkspace_ParseComponent(const cJSON *source, BWorkspaceComponent *destination,
                                      BDiagnosticList *diagnostics)
{
    const char *const fields[] = {"type", "version", "required", "data"};
    cJSON *type = cJSON_GetObjectItemCaseSensitive(source, "type");
    cJSON *version = cJSON_GetObjectItemCaseSensitive(source, "version");
    cJSON *required = cJSON_GetObjectItemCaseSensitive(source, "required");
    cJSON *data = cJSON_GetObjectItemCaseSensitive(source, "data");

    if (!BWorkspaceDocument_HasOnlyFields(source, fields, 4) || !cJSON_IsString(type) ||
        !cJSON_IsNumber(version) || version->valuedouble != (double)version->valueint ||
        version->valueint <= 0 || !cJSON_IsBool(required) || !cJSON_IsObject(data) ||
        !BWorkspace_IsValidComponentType(type->valuestring)) {
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace contains an invalid component envelope.");
    }

    memset(destination, 0, sizeof(*destination));
    snprintf(destination->type, sizeof(destination->type), "%s", type->valuestring);
    destination->version = version->valueint;
    destination->required = cJSON_IsTrue(required);

    if (strcmp(destination->type, BWORKSPACE_TRANSFORM2D_TYPE) == 0 && destination->version == 1) {
        const char *const dataFields[] = {"x", "y"};
        cJSON *x = cJSON_GetObjectItemCaseSensitive(data, "x");
        cJSON *y = cJSON_GetObjectItemCaseSensitive(data, "y");

        if (!BWorkspaceDocument_HasOnlyFields(data, dataFields, 2) || !cJSON_IsNumber(x) ||
            !cJSON_IsNumber(y) || !isfinite(x->valuedouble) || !isfinite(y->valuedouble)) {
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "Transform2D component data is invalid.");
        }

        destination->kind = BWORKSPACE_COMPONENT_TRANSFORM2D;
        destination->data.transform2d.x = (float)x->valuedouble;
        destination->data.transform2d.y = (float)y->valuedouble;
        return true;
    }

    if (strcmp(destination->type, BWORKSPACE_ASCII_RENDERABLE_TYPE) == 0 &&
        destination->version == 1) {
        const char *const dataFields[] = {"source", "foreground", "background",       "layer",
                                          "anchor", "visible",    "transparentSpaces"};
        cJSON *sourceData = cJSON_GetObjectItemCaseSensitive(data, "source");
        cJSON *foreground = cJSON_GetObjectItemCaseSensitive(data, "foreground");
        cJSON *background = cJSON_GetObjectItemCaseSensitive(data, "background");
        cJSON *layer = cJSON_GetObjectItemCaseSensitive(data, "layer");
        cJSON *anchor = cJSON_GetObjectItemCaseSensitive(data, "anchor");
        cJSON *visible = cJSON_GetObjectItemCaseSensitive(data, "visible");
        cJSON *transparentSpaces = cJSON_GetObjectItemCaseSensitive(data, "transparentSpaces");
        BAsciiRenderable *renderable = &destination->data.asciiRenderable;

        if (!BWorkspaceDocument_HasOnlyFields(data, dataFields, 7) || !cJSON_IsObject(sourceData) ||
            !cJSON_IsNumber(layer) || layer->valuedouble != (double)layer->valueint ||
            layer->valueint < SHRT_MIN || layer->valueint > SHRT_MAX || !cJSON_IsString(anchor) ||
            !cJSON_IsBool(visible) || !cJSON_IsBool(transparentSpaces) ||
            !BWorkspace_ParseColor(foreground, &renderable->foreground) ||
            !BWorkspace_ParseColor(background, &renderable->background)) {
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "ASCII Renderable component data is invalid.");
        }

        const char *const glyphFields[] = {"kind", "glyph"};
        const char *const spriteFields[] = {"kind", "path"};
        cJSON *sourceKind = cJSON_GetObjectItemCaseSensitive(sourceData, "kind");

        if (!cJSON_IsString(sourceKind))
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "ASCII Renderable source is invalid.");

        if (strcmp(sourceKind->valuestring, "glyph") == 0) {
            cJSON *glyph = cJSON_GetObjectItemCaseSensitive(sourceData, "glyph");

            if (!BWorkspaceDocument_HasOnlyFields(sourceData, glyphFields, 2) ||
                !cJSON_IsString(glyph) || strlen(glyph->valuestring) != 1 ||
                (unsigned char)glyph->valuestring[0] < 0x20 ||
                (unsigned char)glyph->valuestring[0] > 0x7e) {
                return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                               "ASCII glyph source is invalid.");
            }

            renderable->sourceKind = BASCII_SOURCE_GLYPH;
            renderable->glyph = glyph->valuestring[0];
        } else if (strcmp(sourceKind->valuestring, "text-sprite") == 0) {
            cJSON *path = cJSON_GetObjectItemCaseSensitive(sourceData, "path");

            if (!BWorkspaceDocument_HasOnlyFields(sourceData, spriteFields, 2) ||
                !cJSON_IsString(path) || !BWorkspace_IsRelativeAssetPath(path->valuestring)) {
                return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                               "Text Sprite source path is invalid.");
            }

            renderable->sourceKind = BASCII_SOURCE_TEXT_SPRITE;
            snprintf(renderable->textSprite.path, sizeof(renderable->textSprite), "%s",
                     path->valuestring);
        } else {
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "ASCII Renderable source kind is unsupported.");
        }

        if (strcmp(anchor->valuestring, "bottom-center") == 0)
            renderable->anchor = BASCII_ANCHOR_BOTTOM_CENTER;
        else if (strcmp(anchor->valuestring, "center") == 0)
            renderable->anchor = BASCII_ANCHOR_CENTER;
        else if (strcmp(anchor->valuestring, "top-left") == 0)
            renderable->anchor = BASCII_ANCHOR_TOP_LEFT;
        else
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "ASCII Renderable anchor is unsupported.");

        renderable->layer = (short)layer->valueint;
        renderable->visible = cJSON_IsTrue(visible);
        renderable->transparentSpaces = cJSON_IsTrue(transparentSpaces);
        destination->kind = BWORKSPACE_COMPONENT_ASCII_RENDERABLE;
        return true;
    }

    if (destination->required)
        return BWorkspaceDocument_Fail(
            diagnostics, BDIAGNOSTIC_UNSUPPORTED_VERSION,
            "A required Workspace component type or version is unsupported.");

    char *unknownJson = cJSON_PrintUnformatted(data);

    if (unknownJson == 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                       "Could not preserve unknown component data.");

    size_t length = strlen(unknownJson);

    if (length > BWORKSPACE_UNKNOWN_DATA_MAX) {
        cJSON_free(unknownJson);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Unknown component data exceeds its supported size.");
    }

    destination->data.unknownDataJson = (char *)malloc(length + 1);

    if (destination->data.unknownDataJson == 0) {
        cJSON_free(unknownJson);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                       "Out of memory while preserving unknown component data.");
    }

    memcpy(destination->data.unknownDataJson, unknownJson, length + 1);
    cJSON_free(unknownJson);
    destination->kind = BWORKSPACE_COMPONENT_UNKNOWN;
    return true;
}

bool BWorkspaceDocument_Load(const char *workspacePath, BWorkspaceDocument *destination,
                             BDiagnosticList *diagnostics)
{
    BWorkspaceDocument_ClearError(diagnostics);

    if (workspacePath == 0 || destination == 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace path and output are required.");

    char *contents = BWorkspaceDocument_ReadFile(workspacePath, diagnostics);

    if (contents == 0)
        return false;

    cJSON *root = cJSON_Parse(contents);
    free(contents);

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace contains invalid JSON.");
    }

    cJSON *schemaVersion = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *identifier = cJSON_GetObjectItemCaseSensitive(root, "identifier");
    cJSON *entities = cJSON_GetObjectItemCaseSensitive(root, "entities");

    if (!cJSON_IsNumber(schemaVersion) || !cJSON_IsString(name) || !cJSON_IsString(identifier) ||
        !cJSON_IsArray(entities) || schemaVersion->valuedouble != (double)schemaVersion->valueint) {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_DATA,
            "Workspace is missing required fields or contains incorrect field types.");
    }

    int sourceVersion = schemaVersion->valueint;

    if (sourceVersion != BWORKSPACE_LEGACY_SCHEMA_VERSION &&
        sourceVersion != BWORKSPACE_PREVIOUS_SCHEMA_VERSION &&
        sourceVersion != BWORKSPACE_SCHEMA_VERSION) {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_UNSUPPORTED_VERSION,
                                       "Unsupported Workspace schema version.");
    }

    const char *const legacyFields[] = {"schemaVersion", "name", "identifier", "entities"};
    const char *const currentFields[] = {"schemaVersion", "name", "identifier", "nextEntityId",
                                         "entities"};
    const char *const *rootFields =
        sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION ? legacyFields : currentFields;
    size_t rootFieldCount = sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION ? 4 : 5;

    if (!BWorkspaceDocument_HasOnlyFields(root, rootFields, rootFieldCount)) {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Workspace contains fields this schema cannot preserve.");
    }

    if (strlen(name->valuestring) >= BWORKSPACE_NAME_MAX ||
        strlen(identifier->valuestring) >= BWORKSPACE_IDENTIFIER_MAX) {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_DATA,
            "Workspace contains a string that exceeds its supported length.");
    }

    BWorkspaceDocument workspace;
    BWorkspaceDocument_Init(&workspace);

    if (!BWorkspaceDocument_CreateDefault(&workspace, name->valuestring, identifier->valuestring,
                                          diagnostics)) {
        cJSON_Delete(root);
        return false;
    }

    workspace.sourceSchemaVersion = sourceVersion;

    if (sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION) {
        if (cJSON_GetArraySize(entities) != 0) {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "Legacy Workspace entity content is unsupported.");
        }
    } else {
        cJSON *nextEntityId = cJSON_GetObjectItemCaseSensitive(root, "nextEntityId");

        if (!cJSON_IsString(nextEntityId) || nextEntityId->valuestring[0] == '\0') {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "Workspace next entity ID is missing or invalid.");
        }

        char *idEnd = 0;
        errno = 0;
        unsigned long long nextId = strtoull(nextEntityId->valuestring, &idEnd, 10);

        if (errno != 0 || idEnd == nextEntityId->valuestring || *idEnd != '\0' || nextId == 0) {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "Workspace next entity ID is invalid.");
        }

        int entityCount = cJSON_GetArraySize(entities);

        if (entityCount < 0 || entityCount > BWORKSPACE_ENTITY_MAX) {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "Workspace contains too many entities.");
        }

        workspace.nextEntityId = nextId;

        if (!BWorkspaceDocument_Reserve(&workspace, (size_t)entityCount, diagnostics)) {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return false;
        }

        const char *const previousEntityFields[] = {"id", "name", "enabled"};
        const char *const currentEntityFields[] = {"id", "name", "enabled", "components"};

        for (int i = 0; i < entityCount; ++i) {
            cJSON *sourceEntity = cJSON_GetArrayItem(entities, i);
            cJSON *id = cJSON_GetObjectItemCaseSensitive(sourceEntity, "id");
            cJSON *entityName = cJSON_GetObjectItemCaseSensitive(sourceEntity, "name");
            cJSON *enabled = cJSON_GetObjectItemCaseSensitive(sourceEntity, "enabled");
            cJSON *components = cJSON_GetObjectItemCaseSensitive(sourceEntity, "components");
            const char *const *entityFields = sourceVersion == BWORKSPACE_SCHEMA_VERSION
                                                  ? currentEntityFields
                                                  : previousEntityFields;
            size_t entityFieldCount = sourceVersion == BWORKSPACE_SCHEMA_VERSION ? 4 : 3;

            if (!BWorkspaceDocument_HasOnlyFields(sourceEntity, entityFields, entityFieldCount) ||
                !cJSON_IsString(id) || !cJSON_IsString(entityName) || !cJSON_IsBool(enabled) ||
                (sourceVersion == BWORKSPACE_SCHEMA_VERSION && !cJSON_IsArray(components)) ||
                strlen(id->valuestring) >= BWORKSPACE_ENTITY_ID_MAX ||
                strlen(entityName->valuestring) >= BWORKSPACE_ENTITY_NAME_MAX) {
                cJSON_Delete(root);
                BWorkspaceDocument_Destroy(&workspace);
                return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                               "Workspace contains an invalid entity.");
            }

            BWorkspaceEntity *destination = &workspace.entities[i];
            snprintf(destination->id, sizeof(destination->id), "%s", id->valuestring);
            snprintf(destination->name, sizeof(destination->name), "%s", entityName->valuestring);
            destination->enabled = cJSON_IsTrue(enabled);

            if (sourceVersion == BWORKSPACE_SCHEMA_VERSION) {
                int componentCount = cJSON_GetArraySize(components);

                if (componentCount < 0 || componentCount > BWORKSPACE_ENTITY_COMPONENT_MAX ||
                    workspace.componentCount + (size_t)componentCount > BWORKSPACE_COMPONENT_MAX ||
                    !BWorkspaceEntity_ReserveComponents(destination, (size_t)componentCount,
                                                        diagnostics)) {
                    cJSON_Delete(root);
                    workspace.entityCount = (size_t)i + 1;
                    BWorkspaceDocument_Destroy(&workspace);
                    return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                                   "Workspace contains too many components.");
                }

                for (int componentIndex = 0; componentIndex < componentCount; ++componentIndex) {
                    BWorkspaceComponent *component = &destination->components[componentIndex];

                    if (!BWorkspace_ParseComponent(cJSON_GetArrayItem(components, componentIndex),
                                                   component, diagnostics)) {
                        if (diagnostics != 0 && diagnostics->count > 0) {
                            BDiagnostic *diagnostic = &diagnostics->items[diagnostics->count - 1];
                            snprintf(diagnostic->entityId, sizeof(diagnostic->entityId), "%s",
                                     destination->id);
                            cJSON *failedType = cJSON_GetObjectItemCaseSensitive(
                                cJSON_GetArrayItem(components, componentIndex), "type");

                            if (cJSON_IsString(failedType))
                                snprintf(diagnostic->componentType,
                                         sizeof(diagnostic->componentType), "%s",
                                         failedType->valuestring);
                        }

                        destination->componentCount = (size_t)componentIndex;
                        cJSON_Delete(root);
                        workspace.entityCount = (size_t)i + 1;
                        BWorkspaceDocument_Destroy(&workspace);
                        return false;
                    }

                    destination->componentCount += 1;
                    workspace.componentCount += 1;
                }
            }
        }

        workspace.entityCount = (size_t)entityCount;
    }

    cJSON_Delete(root);

    if (!BWorkspaceDocument_Validate(&workspace, diagnostics)) {
        BWorkspaceDocument_Destroy(&workspace);
        return false;
    }

    for (size_t entityIndex = 0; entityIndex < workspace.entityCount; ++entityIndex) {
        const BWorkspaceEntity *entity = &workspace.entities[entityIndex];

        for (size_t componentIndex = 0; componentIndex < entity->componentCount; ++componentIndex) {
            const BWorkspaceComponent *component = &entity->components[componentIndex];

            if (component->kind != BWORKSPACE_COMPONENT_UNKNOWN)
                continue;

            BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_WARNING, BDIAGNOSTIC_UNSUPPORTED_VERSION,
                                "Optional Workspace component is unsupported and was preserved.",
                                workspacePath);

            if (diagnostics != 0 && diagnostics->count > 0) {
                BDiagnostic *warning = &diagnostics->items[diagnostics->count - 1];
                snprintf(warning->entityId, sizeof(warning->entityId), "%s", entity->id);
                snprintf(warning->componentType, sizeof(warning->componentType), "%s",
                         component->type);
            }
        }
    }

    BWorkspaceDocument_Swap(destination, &workspace);
    BWorkspaceDocument_Destroy(&workspace);
    return true;
}

static void BWorkspace_FormatColor(char output[10], BAsciiColor color)
{
    snprintf(output, 10, "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
}

static cJSON *BWorkspace_ComponentDataToJson(const BWorkspaceComponent *component,
                                             BDiagnosticList *diagnostics)
{
    if (component->kind == BWORKSPACE_COMPONENT_UNKNOWN) {
        cJSON *data = cJSON_Parse(component->data.unknownDataJson);

        if (!cJSON_IsObject(data)) {
            cJSON_Delete(data);
            BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                    "Preserved unknown component data is no longer valid JSON.");
            return 0;
        }

        return data;
    }

    cJSON *data = cJSON_CreateObject();

    if (data == 0) {
        BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                "Out of memory while serializing component data.");
        return 0;
    }

    if (component->kind == BWORKSPACE_COMPONENT_TRANSFORM2D) {
        cJSON_AddNumberToObject(data, "x", component->data.transform2d.x);
        cJSON_AddNumberToObject(data, "y", component->data.transform2d.y);
        return data;
    }

    const BAsciiRenderable *renderable = &component->data.asciiRenderable;
    cJSON *source = cJSON_CreateObject();

    if (source == 0) {
        cJSON_Delete(data);
        BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                "Out of memory while serializing an ASCII source.");
        return 0;
    }

    if (renderable->sourceKind == BASCII_SOURCE_GLYPH) {
        char glyph[2] = {renderable->glyph, '\0'};
        cJSON_AddStringToObject(source, "kind", "glyph");
        cJSON_AddStringToObject(source, "glyph", glyph);
    } else {
        cJSON_AddStringToObject(source, "kind", "text-sprite");
        cJSON_AddStringToObject(source, "path", renderable->textSprite.path);
    }

    char foreground[10];
    char background[10];
    BWorkspace_FormatColor(foreground, renderable->foreground);
    BWorkspace_FormatColor(background, renderable->background);
    const char *anchor = renderable->anchor == BASCII_ANCHOR_CENTER     ? "center"
                         : renderable->anchor == BASCII_ANCHOR_TOP_LEFT ? "top-left"
                                                                        : "bottom-center";
    cJSON_AddItemToObject(data, "source", source);
    cJSON_AddStringToObject(data, "foreground", foreground);
    cJSON_AddStringToObject(data, "background", background);
    cJSON_AddNumberToObject(data, "layer", renderable->layer);
    cJSON_AddStringToObject(data, "anchor", anchor);
    cJSON_AddBoolToObject(data, "visible", renderable->visible);
    cJSON_AddBoolToObject(data, "transparentSpaces", renderable->transparentSpaces);
    return data;
}

static bool BWorkspaceDocument_WriteJson(const BWorkspaceDocument *workspace, const char *path,
                                         BDiagnosticList *error)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *entities = cJSON_CreateArray();

    if (root == 0 || entities == 0) {
        cJSON_Delete(root);
        cJSON_Delete(entities);
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO,
                                       "Out of memory while creating the Workspace.");
    }

    char nextEntityId[32];
    snprintf(nextEntityId, sizeof(nextEntityId), "%llu", workspace->nextEntityId);
    cJSON_AddNumberToObject(root, "schemaVersion", workspace->schemaVersion);
    cJSON_AddStringToObject(root, "name", workspace->name);
    cJSON_AddStringToObject(root, "identifier", workspace->identifier);
    cJSON_AddStringToObject(root, "nextEntityId", nextEntityId);

    for (size_t i = 0; i < workspace->entityCount; ++i) {
        const BWorkspaceEntity *entity = &workspace->entities[i];
        cJSON *serializedEntity = cJSON_CreateObject();

        if (serializedEntity == 0) {
            cJSON_Delete(root);
            cJSON_Delete(entities);
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO,
                                           "Out of memory while serializing entities.");
        }

        cJSON_AddStringToObject(serializedEntity, "id", entity->id);
        cJSON_AddStringToObject(serializedEntity, "name", entity->name);
        cJSON_AddBoolToObject(serializedEntity, "enabled", entity->enabled);
        cJSON *components = cJSON_CreateArray();

        if (components == 0) {
            cJSON_Delete(serializedEntity);
            cJSON_Delete(root);
            cJSON_Delete(entities);
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_OUT_OF_MEMORY,
                                           "Out of memory while serializing components.");
        }

        for (size_t componentIndex = 0; componentIndex < entity->componentCount; ++componentIndex) {
            const BWorkspaceComponent *component = &entity->components[componentIndex];
            cJSON *serializedComponent = cJSON_CreateObject();
            cJSON *data = BWorkspace_ComponentDataToJson(component, error);

            if (serializedComponent == 0 || data == 0) {
                cJSON_Delete(serializedComponent);
                cJSON_Delete(data);
                cJSON_Delete(components);
                cJSON_Delete(serializedEntity);
                cJSON_Delete(root);
                cJSON_Delete(entities);
                return false;
            }

            cJSON_AddStringToObject(serializedComponent, "type", component->type);
            cJSON_AddNumberToObject(serializedComponent, "version", component->version);
            cJSON_AddBoolToObject(serializedComponent, "required", component->required);
            cJSON_AddItemToObject(serializedComponent, "data", data);
            cJSON_AddItemToArray(components, serializedComponent);
        }

        cJSON_AddItemToObject(serializedEntity, "components", components);
        cJSON_AddItemToArray(entities, serializedEntity);
    }

    cJSON_AddItemToObject(root, "entities", entities);
    char *json = cJSON_Print(root);
    cJSON_Delete(root);

    if (json == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not serialize the Workspace.");

    FILE *file = fopen(path, "wb");

    if (file == 0) {
        cJSON_free(json);
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO,
                                       "Could not create the Workspace file.");
    }

    size_t length = strlen(json);
    bool succeeded = fwrite(json, 1, length, file) == length && fwrite("\n", 1, 1, file) == 1;
    bool closed = fclose(file) == 0;
    cJSON_free(json);

    if (!succeeded || !closed)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO,
                                       "Could not write the complete Workspace file.");

    return true;
}

bool BWorkspaceDocument_Save(const BWorkspaceDocument *workspace, const char *workspacePath,
                             BDiagnosticList *error)
{
    BWorkspaceDocument_ClearError(error);

    if (workspacePath == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace path is required.");

    if (!BWorkspaceDocument_Validate(workspace, error))
        return false;

    char temporary[BDIAGNOSTIC_PATH_MAX + 8];
    char backup[BDIAGNOSTIC_PATH_MAX + 8];
    int temporaryLength = snprintf(temporary, sizeof(temporary), "%s.tmp", workspacePath);
    int backupLength = snprintf(backup, sizeof(backup), "%s.bak", workspacePath);

    if (temporaryLength < 0 || (size_t)temporaryLength >= sizeof(temporary) || backupLength < 0 ||
        (size_t)backupLength >= sizeof(backup)) {
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT,
                                       "Workspace path is too long.");
    }

    remove(temporary);

    if (!BWorkspaceDocument_WriteJson(workspace, temporary, error))
        return false;

    FILE *existing = fopen(workspacePath, "rb");
    bool destinationExists = existing != 0;

    if (existing != 0)
        fclose(existing);

    if (destinationExists) {
        remove(backup);

        if (rename(workspacePath, backup) != 0) {
            remove(temporary);
            return BWorkspaceDocument_Fail(
                error, BDIAGNOSTIC_IO, "Could not back up the existing Workspace before saving.");
        }
    }

    if (rename(temporary, workspacePath) != 0) {
        if (destinationExists)
            rename(backup, workspacePath);

        remove(temporary);
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO,
                                       "Could not replace the Workspace file.");
    }

    return true;
}
