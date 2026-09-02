#include "BWorkspace.h"

#include "cJSON.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void BWorkspaceDocument_ClearError(BDiagnosticList* diagnostics)
{
    BDiagnosticList_Clear(diagnostics);
}

static bool BWorkspaceDocument_Fail(
    BDiagnosticList* diagnostics,
    BDiagnosticCode code,
    const char* message
)
{
    BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, code, message, 0);
    return false;
}

static bool BWorkspaceDocument_Reserve(
    BWorkspaceDocument* document,
    size_t required,
    BDiagnosticList* diagnostics
)
{
    if (required <= document->entityCapacity)
        return true;

    if (required > BWORKSPACE_ENTITY_MAX)
        return BWorkspaceDocument_Fail(
            diagnostics,
            BDIAGNOSTIC_INVALID_DATA,
            "Workspace entity capacity has been reached."
        );

    size_t capacity = document->entityCapacity == 0 ? 8 : document->entityCapacity;

    while (capacity < required)
    {
        if (capacity >= BWORKSPACE_ENTITY_MAX / 2)
        {
            capacity = BWORKSPACE_ENTITY_MAX;
            break;
        }

        capacity *= 2;
    }

    BWorkspaceEntity* entities = (BWorkspaceEntity*)realloc(
        document->entities,
        capacity * sizeof(*entities)
    );

    if (entities == 0)
        return BWorkspaceDocument_Fail(
            diagnostics,
            BDIAGNOSTIC_OUT_OF_MEMORY,
            "Out of memory while growing the Workspace entity list."
        );

    memset(
        entities + document->entityCapacity,
        0,
        (capacity - document->entityCapacity) * sizeof(*entities)
    );
    document->entities = entities;
    document->entityCapacity = capacity;
    return true;
}

static bool BWorkspaceDocument_IsValidIdentifier(const char* identifier)
{
    if (identifier == 0 || identifier[0] == '\0')
        return false;

    if (!(isalpha((unsigned char)identifier[0]) || identifier[0] == '_'))
        return false;

    for (size_t i = 1; identifier[i] != '\0'; ++i)
    {
        if (!(isalnum((unsigned char)identifier[i]) || identifier[i] == '_'))
            return false;
    }

    return true;
}

static bool BWorkspaceDocument_HasOnlyFields(
    const cJSON* object,
    const char* const* fields,
    size_t fieldCount
)
{
    if (!cJSON_IsObject(object))
        return false;

    for (const cJSON* item = object->child; item != 0; item = item->next)
    {
        bool known = false;

        for (const cJSON* previous = object->child; previous != item; previous = previous->next)
        {
            if (strcmp(previous->string, item->string) == 0)
                return false;
        }

        for (size_t i = 0; i < fieldCount; ++i)
        {
            if (strcmp(item->string, fields[i]) == 0)
            {
                known = true;
                break;
            }
        }

        if (!known)
            return false;
    }

    return true;
}

static char* BWorkspaceDocument_ReadFile(const char* path, BDiagnosticList* error)
{
    FILE* file = fopen(path, "rb");

    if (file == 0)
    {
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not open the Workspace file.");
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not measure the Workspace file.");
        return 0;
    }

    long length = ftell(file);

    if (length < 0 || (uintmax_t)length > SIZE_MAX - 1 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not read the Workspace file.");
        return 0;
    }

    size_t fileLength = (size_t)length;
    char* contents = (char*)calloc(fileLength + 1, 1);

    if (contents == 0)
    {
        fclose(file);
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Out of memory while reading the Workspace.");
        return 0;
    }

    size_t bytesRead = fread(contents, 1, fileLength, file);
    fclose(file);

    if (bytesRead != fileLength)
    {
        free(contents);
        BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not read the complete Workspace file.");
        return 0;
    }

    return contents;
}

void BWorkspaceDocument_Init(BWorkspaceDocument* document)
{
    if (document != 0)
        memset(document, 0, sizeof(*document));
}

void BWorkspaceDocument_Destroy(BWorkspaceDocument* document)
{
    if (document == 0)
        return;

    free(document->entities);
    memset(document, 0, sizeof(*document));
}

void BWorkspaceDocument_Swap(BWorkspaceDocument* left, BWorkspaceDocument* right)
{
    if (left == 0 || right == 0 || left == right)
        return;

    BWorkspaceDocument temporary = *left;
    *left = *right;
    *right = temporary;
}

bool BWorkspaceDocument_CreateDefault(
    BWorkspaceDocument* document,
    const char* name,
    const char* identifier,
    BDiagnosticList* diagnostics
)
{
    BWorkspaceDocument_ClearError(diagnostics);

    if (document == 0 || name == 0 || identifier == 0)
        return BWorkspaceDocument_Fail(
            diagnostics,
            BDIAGNOSTIC_INVALID_ARGUMENT,
            "Workspace destination, name, and identifier are required."
        );

    if (strlen(name) >= BWORKSPACE_NAME_MAX || strlen(identifier) >= BWORKSPACE_IDENTIFIER_MAX)
        return BWorkspaceDocument_Fail(
            diagnostics,
            BDIAGNOSTIC_INVALID_DATA,
            "Workspace name or identifier is too long."
        );

    BWorkspaceDocument workspace;
    BWorkspaceDocument_Init(&workspace);
    workspace.schemaVersion = BWORKSPACE_SCHEMA_VERSION;
    workspace.nextEntityId = 1;

    snprintf(workspace.name, sizeof(workspace.name), "%s", name);
    snprintf(workspace.identifier, sizeof(workspace.identifier), "%s", identifier);

    if (!BWorkspaceDocument_Validate(&workspace, diagnostics))
        return false;

    BWorkspaceDocument_Swap(document, &workspace);
    BWorkspaceDocument_Destroy(&workspace);
    return true;
}

bool BWorkspaceDocument_Clone(
    const BWorkspaceDocument* source,
    BWorkspaceDocument* destination,
    BDiagnosticList* diagnostics
)
{
    BWorkspaceDocument_ClearError(diagnostics);

    if (source == 0 || destination == 0 || source == destination)
        return BWorkspaceDocument_Fail(
            diagnostics,
            BDIAGNOSTIC_INVALID_ARGUMENT,
            "Distinct Workspace source and destination documents are required."
        );

    if (!BWorkspaceDocument_Validate(source, diagnostics))
        return false;

    BWorkspaceDocument clone;
    BWorkspaceDocument_Init(&clone);
    clone.schemaVersion = source->schemaVersion;
    clone.nextEntityId = source->nextEntityId;
    snprintf(clone.name, sizeof(clone.name), "%s", source->name);
    snprintf(clone.identifier, sizeof(clone.identifier), "%s", source->identifier);

    if (!BWorkspaceDocument_Reserve(&clone, source->entityCount, diagnostics))
    {
        BWorkspaceDocument_Destroy(&clone);
        return false;
    }

    if (source->entityCount > 0)
        memcpy(clone.entities, source->entities, source->entityCount * sizeof(*source->entities));

    clone.entityCount = source->entityCount;
    BWorkspaceDocument_Swap(destination, &clone);
    BWorkspaceDocument_Destroy(&clone);
    return true;
}

bool BWorkspaceDocument_Validate(const BWorkspaceDocument* workspace, BDiagnosticList* error)
{
    BWorkspaceDocument_ClearError(error);

    if (workspace == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT, "Workspace was null.");

    if (workspace->schemaVersion != BWORKSPACE_SCHEMA_VERSION)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_UNSUPPORTED_VERSION, "Unsupported Workspace schema version.");

    if (workspace->name[0] == '\0')
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Workspace name is required.");

    if (!BWorkspaceDocument_IsValidIdentifier(workspace->identifier))
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Workspace identifier must be a valid identifier.");

    if (workspace->entityCapacity > BWORKSPACE_ENTITY_MAX ||
        (workspace->entityCapacity > 0 && workspace->entities == 0) ||
        (workspace->entityCapacity == 0 && workspace->entities != 0) ||
        workspace->entityCount > BWORKSPACE_ENTITY_MAX ||
        workspace->entityCount > workspace->entityCapacity ||
        workspace->nextEntityId == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Workspace entity state is invalid.");

    for (size_t i = 0; i < workspace->entityCount; ++i)
    {
        const BWorkspaceEntity* entity = &workspace->entities[i];

        if (entity->id[0] == '\0' || entity->name[0] == '\0')
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Workspace entity identity and name are required.");

        for (size_t other = i + 1; other < workspace->entityCount; ++other)
        {
            if (strcmp(entity->id, workspace->entities[other].id) == 0)
                return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Workspace entity IDs must be unique.");
        }
    }

    return true;
}

bool BWorkspaceDocument_AddEntity(
    BWorkspaceDocument* document,
    const char* name,
    size_t* outIndex,
    BDiagnosticList* error
)
{
    BWorkspaceDocument_ClearError(error);

    if (document == 0 || name == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT, "Workspace and entity name are required.");

    if (name[0] == '\0' || strlen(name) >= BWORKSPACE_ENTITY_NAME_MAX)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Entity name is empty or too long.");

    if (document->entityCount >= BWORKSPACE_ENTITY_MAX || document->nextEntityId == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Workspace entity capacity has been reached.");

    if (!BWorkspaceDocument_Reserve(document, document->entityCount + 1, error))
        return false;

    size_t index = document->entityCount;
    BWorkspaceEntity* entity = &document->entities[index];
    bool unique = false;

    while (!unique)
    {
        int written = snprintf(entity->id, sizeof(entity->id), "entity-%016llx", document->nextEntityId);

        if (written < 0 || (size_t)written >= sizeof(entity->id))
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Could not generate an entity ID.");

        unique = true;

        for (size_t i = 0; i < document->entityCount; ++i)
        {
            if (strcmp(entity->id, document->entities[i].id) == 0)
            {
                unique = false;
                break;
            }
        }

        if (!unique)
        {
            if (document->nextEntityId == ULLONG_MAX)
                return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Workspace entity IDs are exhausted.");

            document->nextEntityId += 1;
        }
    }

    if (document->nextEntityId == ULLONG_MAX)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_DATA, "Workspace entity IDs are exhausted.");

    snprintf(entity->name, sizeof(entity->name), "%s", name);
    entity->enabled = true;
    document->entityCount += 1;
    document->nextEntityId += 1;

    if (outIndex != 0)
        *outIndex = index;

    return true;
}

bool BWorkspaceDocument_RemoveEntity(BWorkspaceDocument* document, size_t index, BDiagnosticList* error)
{
    BWorkspaceDocument_ClearError(error);

    if (document == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT, "Workspace is required.");

    if (index >= document->entityCount)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT, "Entity index is outside the Workspace.");

    for (size_t i = index + 1; i < document->entityCount; ++i)
        document->entities[i - 1] = document->entities[i];

    document->entityCount -= 1;
    memset(&document->entities[document->entityCount], 0, sizeof(BWorkspaceEntity));
    return true;
}

bool BWorkspaceDocument_Load(
    const char* workspacePath,
    BWorkspaceDocument* destination,
    BDiagnosticList* diagnostics
)
{
    BWorkspaceDocument_ClearError(diagnostics);

    if (workspacePath == 0 || destination == 0)
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Workspace path and output are required.");

    char* contents = BWorkspaceDocument_ReadFile(workspacePath, diagnostics);

    if (contents == 0)
        return false;

    cJSON* root = cJSON_Parse(contents);
    free(contents);

    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Workspace contains invalid JSON.");
    }

    cJSON* schemaVersion = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    cJSON* name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON* identifier = cJSON_GetObjectItemCaseSensitive(root, "identifier");
    cJSON* entities = cJSON_GetObjectItemCaseSensitive(root, "entities");

    if (!cJSON_IsNumber(schemaVersion) || !cJSON_IsString(name) ||
        !cJSON_IsString(identifier) || !cJSON_IsArray(entities) ||
        schemaVersion->valuedouble != (double)schemaVersion->valueint)
    {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Workspace is missing required fields or contains incorrect field types.");
    }

    int sourceVersion = schemaVersion->valueint;

    if (sourceVersion != BWORKSPACE_LEGACY_SCHEMA_VERSION && sourceVersion != BWORKSPACE_SCHEMA_VERSION)
    {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_UNSUPPORTED_VERSION, "Unsupported Workspace schema version.");
    }

    const char* const legacyFields[] = { "schemaVersion", "name", "identifier", "entities" };
    const char* const currentFields[] = { "schemaVersion", "name", "identifier", "nextEntityId", "entities" };
    const char* const* rootFields = sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION ? legacyFields : currentFields;
    size_t rootFieldCount = sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION ? 4 : 5;

    if (!BWorkspaceDocument_HasOnlyFields(root, rootFields, rootFieldCount))
    {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Workspace contains fields this schema cannot preserve.");
    }

    if (strlen(name->valuestring) >= BWORKSPACE_NAME_MAX || strlen(identifier->valuestring) >= BWORKSPACE_IDENTIFIER_MAX)
    {
        cJSON_Delete(root);
        return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Workspace contains a string that exceeds its supported length.");
    }

    BWorkspaceDocument workspace;
    BWorkspaceDocument_Init(&workspace);

    if (!BWorkspaceDocument_CreateDefault(
        &workspace,
        name->valuestring,
        identifier->valuestring,
        diagnostics
    ))
    {
        cJSON_Delete(root);
        return false;
    }

    if (sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION)
    {
        if (cJSON_GetArraySize(entities) != 0)
        {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Legacy Workspace entity content is unsupported.");
        }
    }
    else
    {
        cJSON* nextEntityId = cJSON_GetObjectItemCaseSensitive(root, "nextEntityId");

        if (!cJSON_IsString(nextEntityId) || nextEntityId->valuestring[0] == '\0')
        {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Workspace next entity ID is missing or invalid.");
        }

        char* idEnd = 0;
        errno = 0;
        unsigned long long nextId = strtoull(nextEntityId->valuestring, &idEnd, 10);

        if (errno != 0 || idEnd == nextEntityId->valuestring || *idEnd != '\0' || nextId == 0)
        {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Workspace next entity ID is invalid.");
        }

        int entityCount = cJSON_GetArraySize(entities);

        if (entityCount < 0 || entityCount > BWORKSPACE_ENTITY_MAX)
        {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Workspace contains too many entities.");
        }

        workspace.nextEntityId = nextId;

        if (!BWorkspaceDocument_Reserve(&workspace, (size_t)entityCount, diagnostics))
        {
            cJSON_Delete(root);
            BWorkspaceDocument_Destroy(&workspace);
            return false;
        }

        const char* const entityFields[] = { "id", "name", "enabled" };

        for (int i = 0; i < entityCount; ++i)
        {
            cJSON* sourceEntity = cJSON_GetArrayItem(entities, i);
            cJSON* id = cJSON_GetObjectItemCaseSensitive(sourceEntity, "id");
            cJSON* entityName = cJSON_GetObjectItemCaseSensitive(sourceEntity, "name");
            cJSON* enabled = cJSON_GetObjectItemCaseSensitive(sourceEntity, "enabled");

            if (!BWorkspaceDocument_HasOnlyFields(sourceEntity, entityFields, 3) ||
                !cJSON_IsString(id) || !cJSON_IsString(entityName) || !cJSON_IsBool(enabled) ||
                strlen(id->valuestring) >= BWORKSPACE_ENTITY_ID_MAX ||
                strlen(entityName->valuestring) >= BWORKSPACE_ENTITY_NAME_MAX)
            {
                cJSON_Delete(root);
                BWorkspaceDocument_Destroy(&workspace);
                return BWorkspaceDocument_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Workspace contains an invalid entity.");
            }

            BWorkspaceEntity* destination = &workspace.entities[i];
            snprintf(destination->id, sizeof(destination->id), "%s", id->valuestring);
            snprintf(destination->name, sizeof(destination->name), "%s", entityName->valuestring);
            destination->enabled = cJSON_IsTrue(enabled);
        }

        workspace.entityCount = (size_t)entityCount;
    }

    cJSON_Delete(root);

    if (!BWorkspaceDocument_Validate(&workspace, diagnostics))
    {
        BWorkspaceDocument_Destroy(&workspace);
        return false;
    }

    BWorkspaceDocument_Swap(destination, &workspace);
    BWorkspaceDocument_Destroy(&workspace);
    return true;
}

static bool BWorkspaceDocument_WriteJson(const BWorkspaceDocument* workspace, const char* path, BDiagnosticList* error)
{
    cJSON* root = cJSON_CreateObject();
    cJSON* entities = cJSON_CreateArray();

    if (root == 0 || entities == 0)
    {
        cJSON_Delete(root);
        cJSON_Delete(entities);
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Out of memory while creating the Workspace.");
    }

    char nextEntityId[32];
    snprintf(nextEntityId, sizeof(nextEntityId), "%llu", workspace->nextEntityId);
    cJSON_AddNumberToObject(root, "schemaVersion", workspace->schemaVersion);
    cJSON_AddStringToObject(root, "name", workspace->name);
    cJSON_AddStringToObject(root, "identifier", workspace->identifier);
    cJSON_AddStringToObject(root, "nextEntityId", nextEntityId);

    for (size_t i = 0; i < workspace->entityCount; ++i)
    {
        const BWorkspaceEntity* entity = &workspace->entities[i];
        cJSON* serializedEntity = cJSON_CreateObject();

        if (serializedEntity == 0)
        {
            cJSON_Delete(root);
            cJSON_Delete(entities);
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Out of memory while serializing entities.");
        }

        cJSON_AddStringToObject(serializedEntity, "id", entity->id);
        cJSON_AddStringToObject(serializedEntity, "name", entity->name);
        cJSON_AddBoolToObject(serializedEntity, "enabled", entity->enabled);
        cJSON_AddItemToArray(entities, serializedEntity);
    }

    cJSON_AddItemToObject(root, "entities", entities);
    char* json = cJSON_Print(root);
    cJSON_Delete(root);

    if (json == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not serialize the Workspace.");

    FILE* file = fopen(path, "wb");

    if (file == 0)
    {
        cJSON_free(json);
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not create the Workspace file.");
    }

    size_t length = strlen(json);
    bool succeeded = fwrite(json, 1, length, file) == length && fwrite("\n", 1, 1, file) == 1;
    bool closed = fclose(file) == 0;
    cJSON_free(json);

    if (!succeeded || !closed)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not write the complete Workspace file.");

    return true;
}

bool BWorkspaceDocument_Save(const BWorkspaceDocument* workspace, const char* workspacePath, BDiagnosticList* error)
{
    BWorkspaceDocument_ClearError(error);

    if (workspacePath == 0)
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT, "Workspace path is required.");

    if (!BWorkspaceDocument_Validate(workspace, error))
        return false;

    char temporary[BDIAGNOSTIC_PATH_MAX + 8];
    char backup[BDIAGNOSTIC_PATH_MAX + 8];
    int temporaryLength = snprintf(temporary, sizeof(temporary), "%s.tmp", workspacePath);
    int backupLength = snprintf(backup, sizeof(backup), "%s.bak", workspacePath);

    if (temporaryLength < 0 || (size_t)temporaryLength >= sizeof(temporary) ||
        backupLength < 0 || (size_t)backupLength >= sizeof(backup))
    {
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_INVALID_ARGUMENT, "Workspace path is too long.");
    }

    remove(temporary);

    if (!BWorkspaceDocument_WriteJson(workspace, temporary, error))
        return false;

    FILE* existing = fopen(workspacePath, "rb");
    bool destinationExists = existing != 0;

    if (existing != 0)
        fclose(existing);

    if (destinationExists)
    {
        remove(backup);

        if (rename(workspacePath, backup) != 0)
        {
            remove(temporary);
            return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not back up the existing Workspace before saving.");
        }
    }

    if (rename(temporary, workspacePath) != 0)
    {
        if (destinationExists)
            rename(backup, workspacePath);

        remove(temporary);
        return BWorkspaceDocument_Fail(error, BDIAGNOSTIC_IO, "Could not replace the Workspace file.");
    }

    return true;
}
