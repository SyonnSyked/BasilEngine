#include "BWorkspace.h"

#include "cJSON.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void BWorkspace_ClearError(BProjectError* error)
{
    if (error == 0)
        return;

    error->code = BPROJECT_ERROR_NONE;
    error->message[0] = '\0';
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

    if (root == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace contains invalid JSON.");

    cJSON* schemaVersion = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    cJSON* name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON* identifier = cJSON_GetObjectItemCaseSensitive(root, "identifier");
    cJSON* entities = cJSON_GetObjectItemCaseSensitive(root, "entities");

    if (!cJSON_IsNumber(schemaVersion) || !cJSON_IsString(name) ||
        !cJSON_IsString(identifier) || !cJSON_IsArray(entities))
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace is missing required fields or contains incorrect field types.");
    }

    if (schemaVersion->valuedouble != (double)schemaVersion->valueint)
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace schema version must be an integer.");
    }

    if (schemaVersion->valueint != BWORKSPACE_SCHEMA_VERSION)
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_UNSUPPORTED_VERSION, "Unsupported Workspace schema version.");
    }

    if (cJSON_GetArraySize(entities) != 0)
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace entities are reserved for the next schema revision.");
    }

    if (strlen(name->valuestring) >= BWORKSPACE_NAME_MAX ||
        strlen(identifier->valuestring) >= BWORKSPACE_IDENTIFIER_MAX)
    {
        cJSON_Delete(root);
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Workspace contains a string that exceeds its supported length.");
    }

    BWorkspace workspace = BWorkspace_Default(name->valuestring, identifier->valuestring);
    workspace.schemaVersion = schemaVersion->valueint;
    cJSON_Delete(root);

    if (!BWorkspace_Validate(&workspace, error))
        return false;

    *outWorkspace = workspace;
    return true;
}

bool BWorkspace_Save(const BWorkspace* workspace, const char* workspacePath, BProjectError* error)
{
    BWorkspace_ClearError(error);

    if (workspacePath == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Workspace path is required.");

    if (!BWorkspace_Validate(workspace, error))
        return false;

    cJSON* root = cJSON_CreateObject();
    cJSON* entities = cJSON_CreateArray();

    if (root == 0 || entities == 0)
    {
        cJSON_Delete(root);
        cJSON_Delete(entities);
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Out of memory while creating the Workspace.");
    }

    cJSON_AddNumberToObject(root, "schemaVersion", workspace->schemaVersion);
    cJSON_AddStringToObject(root, "name", workspace->name);
    cJSON_AddStringToObject(root, "identifier", workspace->identifier);
    cJSON_AddItemToObject(root, "entities", entities);

    char* json = cJSON_Print(root);
    cJSON_Delete(root);

    if (json == 0)
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not serialize the Workspace.");

    FILE* file = fopen(workspacePath, "wb");

    if (file == 0)
    {
        cJSON_free(json);
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not create the Workspace file.");
    }

    size_t length = strlen(json);
    bool succeeded = fwrite(json, 1, length, file) == length && fwrite("\n", 1, 1, file) == 1;
    fclose(file);
    cJSON_free(json);

    if (!succeeded)
        return BWorkspace_Fail(error, BPROJECT_ERROR_IO, "Could not write the complete Workspace file.");

    return true;
}
