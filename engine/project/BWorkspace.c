#include "BWorkspace.h"

#include "cJSON.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void BWorkspace_ClearError(BProjectError* error)
{
    if (error != 0)
    {
        error->code = BPROJECT_ERROR_NONE;
        error->message[0] = '\0';
    }
}

static bool BWorkspace_Fail(BProjectError* error, BProjectErrorCode code, const char* message)
{
    if (error != 0)
    {
        error->code = code;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }

    return false;
}

static bool BWorkspace_IsValidIdentifier(const char* identifier)
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

static bool BWorkspace_HasOnlyFields(
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

static char* BWorkspace_ReadFile(const char* path, BProjectError* error)
{
    FILE* file = fopen(path, "rb");

    if (file == 0)
    {
        BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not open the Workspace file.");
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not measure the Workspace file.");
        return 0;
    }

    long length = ftell(file);

    if (length < 0 || (uintmax_t)length > SIZE_MAX - 1 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not read the Workspace file.");
        return 0;
    }

    size_t fileLength = (size_t)length;
    char* contents = (char*)calloc(fileLength + 1, 1);

    if (contents == 0)
    {
        fclose(file);
        BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Out of memory while reading the Workspace.");
        return 0;
    }

    size_t bytesRead = fread(contents, 1, fileLength, file);
    fclose(file);

    if (bytesRead != fileLength)
    {
        free(contents);
        BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not read the complete Workspace file.");
        return 0;
    }

    return contents;
}

BWorkspace BWorkspace_Default(const char* name, const char* identifier)
{
    BWorkspace workspace = { 0 };
    workspace.schemaVersion = BWORKSPACE_SCHEMA_VERSION;
    workspace.nextEntityId = 1;

    if (name != 0 && strlen(name) < sizeof(workspace.name))
        snprintf(workspace.name, sizeof(workspace.name), "%s", name);

    if (identifier != 0 && strlen(identifier) < sizeof(workspace.identifier))
        snprintf(workspace.identifier, sizeof(workspace.identifier), "%s", identifier);

    return workspace;
}

bool BWorkspace_Validate(const BWorkspace* workspace, BProjectError* error)
{
    BWorkspace_ClearError(error);

    if (workspace == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Workspace was null.");

    if (workspace->schemaVersion != BWORKSPACE_SCHEMA_VERSION)
        return BWorkspace_Fail(error, BPROJECT_ERROR_UNSUPPORTED_VERSION, "Unsupported Workspace schema version.");

    if (workspace->name[0] == '\0')
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace name is required.");

    if (!BWorkspace_IsValidIdentifier(workspace->identifier))
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace identifier must be a valid identifier.");

    if (workspace->entityCount > BWORKSPACE_ENTITY_MAX || workspace->nextEntityId == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace entity state is invalid.");

    for (size_t i = 0; i < workspace->entityCount; ++i)
    {
        const BWorkspaceEntity* entity = &workspace->entities[i];

        if (entity->id[0] == '\0' || entity->name[0] == '\0')
            return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace entity identity and name are required.");

        for (size_t other = i + 1; other < workspace->entityCount; ++other)
        {
            if (strcmp(entity->id, workspace->entities[other].id) == 0)
                return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace entity IDs must be unique.");
        }
    }

    return true;
}

bool BWorkspace_AddEntity(
    BWorkspace* workspace,
    const char* name,
    size_t* outIndex,
    BProjectError* error
)
{
    BWorkspace_ClearError(error);

    if (workspace == 0 || name == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Workspace and entity name are required.");

    if (name[0] == '\0' || strlen(name) >= BWORKSPACE_ENTITY_NAME_MAX)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Entity name is empty or too long.");

    if (workspace->entityCount >= BWORKSPACE_ENTITY_MAX || workspace->nextEntityId == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace entity capacity has been reached.");

    size_t index = workspace->entityCount;
    BWorkspaceEntity* entity = &workspace->entities[index];
    bool unique = false;

    while (!unique)
    {
        int written = snprintf(entity->id, sizeof(entity->id), "entity-%016llx", workspace->nextEntityId);

        if (written < 0 || (size_t)written >= sizeof(entity->id))
            return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Could not generate an entity ID.");

        unique = true;

        for (size_t i = 0; i < workspace->entityCount; ++i)
        {
            if (strcmp(entity->id, workspace->entities[i].id) == 0)
            {
                unique = false;
                break;
            }
        }

        if (!unique)
        {
            if (workspace->nextEntityId == ULLONG_MAX)
                return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace entity IDs are exhausted.");

            workspace->nextEntityId += 1;
        }
    }

    if (workspace->nextEntityId == ULLONG_MAX)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace entity IDs are exhausted.");

    snprintf(entity->name, sizeof(entity->name), "%s", name);
    entity->enabled = true;
    workspace->entityCount += 1;
    workspace->nextEntityId += 1;

    if (outIndex != 0)
        *outIndex = index;

    return true;
}

bool BWorkspace_RemoveEntity(BWorkspace* workspace, size_t index, BProjectError* error)
{
    BWorkspace_ClearError(error);

    if (workspace == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Workspace is required.");

    if (index >= workspace->entityCount)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Entity index is outside the Workspace.");

    for (size_t i = index + 1; i < workspace->entityCount; ++i)
        workspace->entities[i - 1] = workspace->entities[i];

    workspace->entityCount -= 1;
    memset(&workspace->entities[workspace->entityCount], 0, sizeof(BWorkspaceEntity));
    return true;
}

bool BWorkspace_Load(const char* workspacePath, BWorkspace* outWorkspace, BProjectError* error)
{
    BWorkspace_ClearError(error);

    if (workspacePath == 0 || outWorkspace == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Workspace path and output are required.");

    char* contents = BWorkspace_ReadFile(workspacePath, error);

    if (contents == 0)
        return false;

    cJSON* root = cJSON_Parse(contents);
    free(contents);

    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace contains invalid JSON.");
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
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace is missing required fields or contains incorrect field types.");
    }

    int sourceVersion = schemaVersion->valueint;

    if (sourceVersion != BWORKSPACE_LEGACY_SCHEMA_VERSION && sourceVersion != BWORKSPACE_SCHEMA_VERSION)
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_UNSUPPORTED_VERSION, "Unsupported Workspace schema version.");
    }

    const char* const legacyFields[] = { "schemaVersion", "name", "identifier", "entities" };
    const char* const currentFields[] = { "schemaVersion", "name", "identifier", "nextEntityId", "entities" };
    const char* const* rootFields = sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION ? legacyFields : currentFields;
    size_t rootFieldCount = sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION ? 4 : 5;

    if (!BWorkspace_HasOnlyFields(root, rootFields, rootFieldCount))
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace contains fields this schema cannot preserve.");
    }

    if (strlen(name->valuestring) >= BWORKSPACE_NAME_MAX || strlen(identifier->valuestring) >= BWORKSPACE_IDENTIFIER_MAX)
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace contains a string that exceeds its supported length.");
    }

    BWorkspace workspace = BWorkspace_Default(name->valuestring, identifier->valuestring);

    if (sourceVersion == BWORKSPACE_LEGACY_SCHEMA_VERSION)
    {
        if (cJSON_GetArraySize(entities) != 0)
        {
            cJSON_Delete(root);
            return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Legacy Workspace entity content is unsupported.");
        }
    }
    else
    {
        cJSON* nextEntityId = cJSON_GetObjectItemCaseSensitive(root, "nextEntityId");

        if (!cJSON_IsString(nextEntityId) || nextEntityId->valuestring[0] == '\0')
        {
            cJSON_Delete(root);
            return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace next entity ID is missing or invalid.");
        }

        char* idEnd = 0;
        errno = 0;
        unsigned long long nextId = strtoull(nextEntityId->valuestring, &idEnd, 10);

        if (errno != 0 || idEnd == nextEntityId->valuestring || *idEnd != '\0' || nextId == 0)
        {
            cJSON_Delete(root);
            return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace next entity ID is invalid.");
        }

        int entityCount = cJSON_GetArraySize(entities);

        if (entityCount < 0 || entityCount > BWORKSPACE_ENTITY_MAX)
        {
            cJSON_Delete(root);
            return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace contains too many entities.");
        }

        workspace.nextEntityId = nextId;
        workspace.entityCount = (size_t)entityCount;
        const char* const entityFields[] = { "id", "name", "enabled" };

        for (int i = 0; i < entityCount; ++i)
        {
            cJSON* sourceEntity = cJSON_GetArrayItem(entities, i);
            cJSON* id = cJSON_GetObjectItemCaseSensitive(sourceEntity, "id");
            cJSON* entityName = cJSON_GetObjectItemCaseSensitive(sourceEntity, "name");
            cJSON* enabled = cJSON_GetObjectItemCaseSensitive(sourceEntity, "enabled");

            if (!BWorkspace_HasOnlyFields(sourceEntity, entityFields, 3) ||
                !cJSON_IsString(id) || !cJSON_IsString(entityName) || !cJSON_IsBool(enabled) ||
                strlen(id->valuestring) >= BWORKSPACE_ENTITY_ID_MAX ||
                strlen(entityName->valuestring) >= BWORKSPACE_ENTITY_NAME_MAX)
            {
                cJSON_Delete(root);
                return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace contains an invalid entity.");
            }

            BWorkspaceEntity* destination = &workspace.entities[i];
            snprintf(destination->id, sizeof(destination->id), "%s", id->valuestring);
            snprintf(destination->name, sizeof(destination->name), "%s", entityName->valuestring);
            destination->enabled = cJSON_IsTrue(enabled);
        }
    }

    cJSON_Delete(root);

    if (!BWorkspace_Validate(&workspace, error))
        return false;

    *outWorkspace = workspace;
    return true;
}

static bool BWorkspace_WriteJson(const BWorkspace* workspace, const char* path, BProjectError* error)
{
    cJSON* root = cJSON_CreateObject();
    cJSON* entities = cJSON_CreateArray();

    if (root == 0 || entities == 0)
    {
        cJSON_Delete(root);
        cJSON_Delete(entities);
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Out of memory while creating the Workspace.");
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
            return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Out of memory while serializing entities.");
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
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not serialize the Workspace.");

    FILE* file = fopen(path, "wb");

    if (file == 0)
    {
        cJSON_free(json);
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not create the Workspace file.");
    }

    size_t length = strlen(json);
    bool succeeded = fwrite(json, 1, length, file) == length && fwrite("\n", 1, 1, file) == 1;
    bool closed = fclose(file) == 0;
    cJSON_free(json);

    if (!succeeded || !closed)
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not write the complete Workspace file.");

    return true;
}

bool BWorkspace_Save(const BWorkspace* workspace, const char* workspacePath, BProjectError* error)
{
    BWorkspace_ClearError(error);

    if (workspacePath == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Workspace path is required.");

    if (!BWorkspace_Validate(workspace, error))
        return false;

    char temporary[BPROJECT_PATH_MAX + 8];
    char backup[BPROJECT_PATH_MAX + 8];
    int temporaryLength = snprintf(temporary, sizeof(temporary), "%s.tmp", workspacePath);
    int backupLength = snprintf(backup, sizeof(backup), "%s.bak", workspacePath);

    if (temporaryLength < 0 || (size_t)temporaryLength >= sizeof(temporary) ||
        backupLength < 0 || (size_t)backupLength >= sizeof(backup))
    {
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Workspace path is too long.");
    }

    remove(temporary);

    if (!BWorkspace_WriteJson(workspace, temporary, error))
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
            return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not back up the existing Workspace before saving.");
        }
    }

    if (rename(temporary, workspacePath) != 0)
    {
        if (destinationExists)
            rename(backup, workspacePath);

        remove(temporary);
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not replace the Workspace file.");
    }

    return true;
}
