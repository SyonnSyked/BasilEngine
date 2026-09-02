#include "BRecentProjects.h"

#include "cJSON.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool BRecentProjects_Fail(BProjectError* error, const char* message)
{
    if (error != 0)
    {
        error->code = BPROJECT_ERROR_IO;
        snprintf(error->message, sizeof(error->message), "%s", message);
    }

    return false;
}

static void BRecentProjects_ClearError(BProjectError* error)
{
    if (error != 0)
    {
        error->code = BPROJECT_ERROR_NONE;
        error->message[0] = '\0';
    }
}

void BRecentProjects_Clear(BRecentProjects* recentProjects)
{
    if (recentProjects != 0)
        memset(recentProjects, 0, sizeof(*recentProjects));
}

bool BRecentProjects_Add(BRecentProjects* recentProjects, const char* manifestPath)
{
    if (recentProjects == 0 || manifestPath == 0 || manifestPath[0] == '\0' ||
        strlen(manifestPath) >= BPROJECT_PATH_MAX)
    {
        return false;
    }

    size_t existing = recentProjects->count;

    for (size_t i = 0; i < recentProjects->count; ++i)
    {
        if (strcmp(recentProjects->paths[i], manifestPath) == 0)
        {
            existing = i;
            break;
        }
    }

    if (existing < recentProjects->count)
    {
        for (size_t i = existing; i > 0; --i)
            memcpy(recentProjects->paths[i], recentProjects->paths[i - 1], BPROJECT_PATH_MAX);
    }
    else
    {
        size_t newCount = recentProjects->count < BRECENT_PROJECTS_MAX ?
            recentProjects->count + 1 : BRECENT_PROJECTS_MAX;

        for (size_t i = newCount - 1; i > 0; --i)
            memcpy(recentProjects->paths[i], recentProjects->paths[i - 1], BPROJECT_PATH_MAX);

        recentProjects->count = newCount;
    }

    snprintf(recentProjects->paths[0], BPROJECT_PATH_MAX, "%s", manifestPath);
    return true;
}

bool BRecentProjects_Remove(BRecentProjects* recentProjects, size_t index)
{
    if (recentProjects == 0 || index >= recentProjects->count)
        return false;

    for (size_t i = index; i + 1 < recentProjects->count; ++i)
        memcpy(recentProjects->paths[i], recentProjects->paths[i + 1], BPROJECT_PATH_MAX);

    --recentProjects->count;
    recentProjects->paths[recentProjects->count][0] = '\0';
    return true;
}

bool BRecentProjects_Load(const char* path, BRecentProjects* recentProjects, BProjectError* error)
{
    BRecentProjects_ClearError(error);

    if (path == 0 || recentProjects == 0)
        return BRecentProjects_Fail(error, "Recent-project path and output are required.");

    BRecentProjects_Clear(recentProjects);
    FILE* file = fopen(path, "rb");

    if (file == 0)
        return true;

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return BRecentProjects_Fail(error, "Could not read recent projects.");
    }

    long length = ftell(file);

    if (length < 0 || (uintmax_t)length > SIZE_MAX - 1 ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return BRecentProjects_Fail(error, "Could not read recent projects.");
    }

    char* contents = (char*)calloc((size_t)length + 1, 1);

    if (contents == 0)
    {
        fclose(file);
        return BRecentProjects_Fail(error, "Out of memory while reading recent projects.");
    }

    bool readSucceeded = fread(contents, 1, (size_t)length, file) == (size_t)length;
    fclose(file);

    if (!readSucceeded)
    {
        free(contents);
        return BRecentProjects_Fail(error, "Could not read recent projects.");
    }

    cJSON* root = cJSON_Parse(contents);
    free(contents);

    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return BRecentProjects_Fail(error, "Recent-project data contains invalid JSON.");
    }

    cJSON* paths = cJSON_GetObjectItemCaseSensitive(root, "projects");

    if (!cJSON_IsArray(paths))
    {
        cJSON_Delete(root);
        return BRecentProjects_Fail(error, "Recent-project data is missing its project list.");
    }

    cJSON* item = 0;
    cJSON_ArrayForEach(item, paths)
    {
        if (recentProjects->count == BRECENT_PROJECTS_MAX)
            break;

        if (cJSON_IsString(item) && item->valuestring != 0 &&
            strlen(item->valuestring) < BPROJECT_PATH_MAX)
        {
            snprintf(recentProjects->paths[recentProjects->count], BPROJECT_PATH_MAX, "%s", item->valuestring);
            ++recentProjects->count;
        }
    }

    cJSON_Delete(root);
    return true;
}

bool BRecentProjects_Save(const char* path, const BRecentProjects* recentProjects, BProjectError* error)
{
    BRecentProjects_ClearError(error);

    if (path == 0 || recentProjects == 0 || recentProjects->count > BRECENT_PROJECTS_MAX)
        return BRecentProjects_Fail(error, "Recent-project path and data are required.");

    cJSON* root = cJSON_CreateObject();
    cJSON* paths = cJSON_CreateArray();

    if (root == 0 || paths == 0)
    {
        cJSON_Delete(root);
        cJSON_Delete(paths);
        return BRecentProjects_Fail(error, "Out of memory while saving recent projects.");
    }

    cJSON_AddNumberToObject(root, "schemaVersion", 1);
    cJSON_AddItemToObject(root, "projects", paths);

    for (size_t i = 0; i < recentProjects->count; ++i)
        cJSON_AddItemToArray(paths, cJSON_CreateString(recentProjects->paths[i]));

    char* json = cJSON_Print(root);
    cJSON_Delete(root);

    if (json == 0)
        return BRecentProjects_Fail(error, "Could not serialize recent projects.");

    FILE* file = fopen(path, "wb");

    if (file == 0)
    {
        cJSON_free(json);
        return BRecentProjects_Fail(error, "Could not open recent-project data for writing.");
    }

    size_t length = strlen(json);
    bool succeeded = fwrite(json, 1, length, file) == length && fwrite("\n", 1, 1, file) == 1;
    fclose(file);
    cJSON_free(json);

    return succeeded || BRecentProjects_Fail(error, "Could not save recent projects.");
}
