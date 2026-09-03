#include "BProject.h"

#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void BProject_ClearError(BProjectError *error)
{
    if (error == 0)
        return;

    error->code = BPROJECT_ERROR_NONE;
    error->message[0] = '\0';
}

static bool BProject_Fail(BProjectError *error, BProjectErrorCode code, const char *message)
{
    if (error != 0) {
        error->code = code;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }

    return false;
}

static bool BProject_IsSupportedCStandard(int standard)
{
    return standard == 90 || standard == 99 || standard == 11 || standard == 17 || standard == 23;
}

static bool BProject_IsSupportedCppStandard(int standard)
{
    return standard == 98 || standard == 11 || standard == 14 || standard == 17 || standard == 20 ||
           standard == 23 || standard == 26;
}

static bool BProject_IsValidIdentifier(const char *identifier)
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

bool BProject_IsPortableRelativePath(const char *path)
{
    if (path == 0 || path[0] == '\0' || path[0] == '/' || path[0] == '\\' ||
        strchr(path, ':') != 0) {
        return false;
    }

    const char *segment = path;

    for (const char *cursor = path;; ++cursor) {
        if (*cursor == '/' || *cursor == '\\' || *cursor == '\0') {
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

static char *BProject_ReadFile(const char *path, BProjectError *error)
{
    FILE *file = fopen(path, "rb");

    if (file == 0) {
        BProject_Fail(error, BPROJECT_ERROR_IO, "Could not open the project manifest.");
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        BProject_Fail(error, BPROJECT_ERROR_IO, "Could not measure the project manifest.");
        return 0;
    }

    long length = ftell(file);

    if (length < 0 || (uintmax_t)length > SIZE_MAX - 1 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        BProject_Fail(error, BPROJECT_ERROR_IO, "Could not read the project manifest.");
        return 0;
    }

    size_t fileLength = (size_t)length;
    char *contents = (char *)calloc(fileLength + 1, 1);

    if (contents == 0) {
        fclose(file);
        BProject_Fail(error, BPROJECT_ERROR_IO, "Out of memory while reading the manifest.");
        return 0;
    }

    size_t bytesRead = fread(contents, 1, fileLength, file);
    fclose(file);

    if (bytesRead != fileLength) {
        free(contents);
        BProject_Fail(error, BPROJECT_ERROR_IO, "Could not read the complete manifest.");
        return 0;
    }

    return contents;
}

BProject BProject_Default(const char *name, const char *identifier)
{
    BProject project = {0};
    project.schemaVersion = BPROJECT_SCHEMA_VERSION;
    project.languageMode = BPROJECT_LANGUAGE_MIXED;
    project.cStandard = 11;
    project.cppStandard = 26;
    snprintf(project.startupWorkspace, sizeof(project.startupWorkspace),
             "workspaces/Main.basilworkspace");

    if (name != 0 && strlen(name) < sizeof(project.name))
        snprintf(project.name, sizeof(project.name), "%s", name);

    if (identifier != 0 && strlen(identifier) < sizeof(project.identifier))
        snprintf(project.identifier, sizeof(project.identifier), "%s", identifier);

    return project;
}

bool BProject_Validate(const BProject *project, BProjectError *error)
{
    BProject_ClearError(error);

    if (project == 0)
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Project was null.");

    if (project->schemaVersion != BPROJECT_SCHEMA_VERSION)
        return BProject_Fail(error, BPROJECT_ERROR_UNSUPPORTED_VERSION,
                             "Unsupported project schema version.");

    if (project->name[0] == '\0')
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST, "Project name is required.");

    if (!BProject_IsValidIdentifier(project->identifier))
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Project identifier must be a valid C identifier.");

    if (project->languageMode < BPROJECT_LANGUAGE_C ||
        project->languageMode > BPROJECT_LANGUAGE_MIXED) {
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Unknown project language mode.");
    }

    if ((project->languageMode == BPROJECT_LANGUAGE_C ||
         project->languageMode == BPROJECT_LANGUAGE_MIXED) &&
        !BProject_IsSupportedCStandard(project->cStandard)) {
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Unsupported C language standard.");
    }

    if ((project->languageMode == BPROJECT_LANGUAGE_CPP ||
         project->languageMode == BPROJECT_LANGUAGE_MIXED) &&
        !BProject_IsSupportedCppStandard(project->cppStandard)) {
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Unsupported C++ language standard.");
    }

    if (!BProject_IsPortableRelativePath(project->startupWorkspace))
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Startup Workspace must be a portable relative path.");

    return true;
}

const char *BProject_LanguageModeToString(BProjectLanguageMode mode)
{
    switch (mode) {
        case BPROJECT_LANGUAGE_C:
            return "c";
        case BPROJECT_LANGUAGE_CPP:
            return "cpp";
        case BPROJECT_LANGUAGE_MIXED:
            return "mixed";
        default:
            return "unknown";
    }
}

bool BProject_LanguageModeFromString(const char *value, BProjectLanguageMode *outMode)
{
    if (value == 0 || outMode == 0)
        return false;

    if (strcmp(value, "c") == 0)
        *outMode = BPROJECT_LANGUAGE_C;
    else if (strcmp(value, "cpp") == 0)
        *outMode = BPROJECT_LANGUAGE_CPP;
    else if (strcmp(value, "mixed") == 0)
        *outMode = BPROJECT_LANGUAGE_MIXED;
    else
        return false;

    return true;
}

bool BProject_Load(const char *manifestPath, BProject *outProject, BProjectError *error)
{
    BProject_ClearError(error);

    if (manifestPath == 0 || outProject == 0)
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT,
                             "Manifest path and output project are required.");

    char *contents = BProject_ReadFile(manifestPath, error);

    if (contents == 0)
        return false;

    cJSON *root = cJSON_Parse(contents);
    free(contents);

    if (root == 0)
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Manifest contains invalid JSON.");

    cJSON *schemaVersion = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *identifier = cJSON_GetObjectItemCaseSensitive(root, "identifier");
    cJSON *languages = cJSON_GetObjectItemCaseSensitive(root, "languages");

    if (!cJSON_IsNumber(schemaVersion) || !cJSON_IsString(name) || !cJSON_IsString(identifier) ||
        !cJSON_IsObject(languages)) {
        cJSON_Delete(root);
        return BProject_Fail(
            error, BPROJECT_ERROR_INVALID_MANIFEST,
            "Manifest is missing required fields or contains incorrect field types.");
    }

    cJSON *mode = cJSON_GetObjectItemCaseSensitive(languages, "mode");
    cJSON *cStandard = cJSON_GetObjectItemCaseSensitive(languages, "cStandard");
    cJSON *cppStandard = cJSON_GetObjectItemCaseSensitive(languages, "cppStandard");

    if (!cJSON_IsString(mode) || !cJSON_IsNumber(cStandard) || !cJSON_IsNumber(cppStandard)) {
        cJSON_Delete(root);
        return BProject_Fail(
            error, BPROJECT_ERROR_INVALID_MANIFEST,
            "Manifest language settings are missing or contain incorrect field types.");
    }

    if (schemaVersion->valuedouble != (double)schemaVersion->valueint ||
        cStandard->valuedouble != (double)cStandard->valueint ||
        cppStandard->valuedouble != (double)cppStandard->valueint) {
        cJSON_Delete(root);
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Manifest versions and language standards must be integers.");
    }

    int sourceSchemaVersion = schemaVersion->valueint;

    if (sourceSchemaVersion != BPROJECT_LEGACY_SCHEMA_VERSION &&
        sourceSchemaVersion != BPROJECT_SCHEMA_VERSION) {
        cJSON_Delete(root);
        return BProject_Fail(error, BPROJECT_ERROR_UNSUPPORTED_VERSION,
                             "Unsupported project schema version.");
    }

    const char *startupFieldName =
        sourceSchemaVersion == BPROJECT_LEGACY_SCHEMA_VERSION ? "startupScene" : "startupWorkspace";
    cJSON *startupWorkspace = cJSON_GetObjectItemCaseSensitive(root, startupFieldName);

    if (!cJSON_IsString(startupWorkspace)) {
        cJSON_Delete(root);
        return BProject_Fail(
            error, BPROJECT_ERROR_INVALID_MANIFEST,
            "Manifest startup Workspace is missing or has an incorrect field type.");
    }

    if (strlen(name->valuestring) >= BPROJECT_NAME_MAX ||
        strlen(identifier->valuestring) >= BPROJECT_IDENTIFIER_MAX ||
        strlen(startupWorkspace->valuestring) >= BPROJECT_PATH_MAX) {
        cJSON_Delete(root);
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Manifest contains a string that exceeds its supported length.");
    }

    BProject project = BProject_Default(name->valuestring, identifier->valuestring);
    project.schemaVersion = BPROJECT_SCHEMA_VERSION;
    project.cStandard = cStandard->valueint;
    project.cppStandard = cppStandard->valueint;
    snprintf(project.startupWorkspace, sizeof(project.startupWorkspace), "%s",
             sourceSchemaVersion == BPROJECT_LEGACY_SCHEMA_VERSION &&
                     startupWorkspace->valuestring[0] == '\0'
                 ? "workspaces/Main.basilworkspace"
                 : startupWorkspace->valuestring);

    bool modeValid = BProject_LanguageModeFromString(mode->valuestring, &project.languageMode);
    cJSON_Delete(root);

    if (!modeValid)
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_MANIFEST,
                             "Manifest contains an unknown language mode.");

    if (!BProject_Validate(&project, error))
        return false;

    *outProject = project;
    return true;
}

bool BProject_Save(const BProject *project, const char *manifestPath, BProjectError *error)
{
    BProject_ClearError(error);

    if (manifestPath == 0)
        return BProject_Fail(error, BPROJECT_ERROR_INVALID_ARGUMENT, "Manifest path is required.");

    if (!BProject_Validate(project, error))
        return false;

    cJSON *root = cJSON_CreateObject();
    cJSON *languages = cJSON_CreateObject();

    if (root == 0 || languages == 0) {
        cJSON_Delete(root);
        cJSON_Delete(languages);
        return BProject_Fail(error, BPROJECT_ERROR_IO,
                             "Out of memory while creating the manifest.");
    }

    cJSON_AddNumberToObject(root, "schemaVersion", project->schemaVersion);
    cJSON_AddStringToObject(root, "name", project->name);
    cJSON_AddStringToObject(root, "identifier", project->identifier);
    cJSON_AddStringToObject(languages, "mode",
                            BProject_LanguageModeToString(project->languageMode));
    cJSON_AddNumberToObject(languages, "cStandard", project->cStandard);
    cJSON_AddNumberToObject(languages, "cppStandard", project->cppStandard);
    cJSON_AddItemToObject(root, "languages", languages);
    cJSON_AddStringToObject(root, "startupWorkspace", project->startupWorkspace);

    char *json = cJSON_Print(root);
    cJSON_Delete(root);

    if (json == 0)
        return BProject_Fail(error, BPROJECT_ERROR_IO, "Could not serialize the project manifest.");

    FILE *file = fopen(manifestPath, "wb");

    if (file == 0) {
        cJSON_free(json);
        return BProject_Fail(error, BPROJECT_ERROR_IO, "Could not create the project manifest.");
    }

    size_t length = strlen(json);
    bool succeeded = fwrite(json, 1, length, file) == length && fwrite("\n", 1, 1, file) == 1;
    fclose(file);
    cJSON_free(json);

    if (!succeeded)
        return BProject_Fail(error, BPROJECT_ERROR_IO,
                             "Could not write the complete project manifest.");

    return true;
}
