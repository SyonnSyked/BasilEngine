#include "BProjectContext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define GET_CWD(buffer, size) _getcwd(buffer, size)
#else
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#define GET_CWD(buffer, size) getcwd(buffer, size)
#endif

static bool Fail(BDiagnosticList *diagnostics, BDiagnosticCode code, const char *message,
                 const char *path)
{
    BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, code, message, path);
    return false;
}

static bool FullPath(const char *path, char *output)
{
#ifdef _WIN32
    return _fullpath(output, path, BPROJECT_PATH_MAX) != NULL;
#else
    return realpath(path, output) != NULL;
#endif
}

static void ParentPath(char *path)
{
    size_t length = strlen(path);
    while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\'))
        path[--length] = '\0';
    while (length > 0 && path[length - 1] != '/' && path[length - 1] != '\\')
        --length;
    if (length > 1)
        path[length - 1] = '\0';
}

static bool FindManifest(const char *directory, char *output, BDiagnosticList *diagnostics)
{
    size_t count = 0;
    char found[BPROJECT_PATH_MAX] = {0};
#ifdef _WIN32
    char pattern[BPROJECT_PATH_MAX];
    snprintf(pattern, sizeof(pattern), "%s/*.basilproject", directory);
    WIN32_FIND_DATAA data;
    HANDLE search = FindFirstFileA(pattern, &data);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                ++count;
                snprintf(found, sizeof(found), "%s/%s", directory, data.cFileName);
            }
        } while (FindNextFileA(search, &data));
        FindClose(search);
    }
#else
    DIR *stream = opendir(directory);
    if (stream != NULL) {
        struct dirent *entry;
        while ((entry = readdir(stream)) != NULL) {
            size_t length = strlen(entry->d_name);
            if (length > 13 && strcmp(entry->d_name + length - 13, ".basilproject") == 0) {
                ++count;
                snprintf(found, sizeof(found), "%s/%s", directory, entry->d_name);
            }
        }
        closedir(stream);
    }
#endif
    if (count > 1)
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                    "Multiple .basilproject manifests were found.", directory);
    if (count == 1) {
        snprintf(output, BPROJECT_PATH_MAX, "%s", found);
        return true;
    }
    return false;
}

void BProjectContext_Init(BProjectContext *context)
{
    if (context != NULL)
        memset(context, 0, sizeof(*context));
}

void BProjectContext_Destroy(BProjectContext *context)
{
    BProjectContext_Init(context);
}

bool BProjectContext_Load(const char *manifestPath, BProjectContext *destination,
                          BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);
    if (manifestPath == NULL || destination == NULL)
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                    "Project manifest and destination context are required.", manifestPath);

    BProjectContext loaded;
    BProjectContext_Init(&loaded);
    if (!FullPath(manifestPath, loaded.manifestPath))
        return Fail(diagnostics, BDIAGNOSTIC_IO, "Could not resolve the Project manifest.",
                    manifestPath);

    snprintf(loaded.projectRoot, sizeof(loaded.projectRoot), "%s", loaded.manifestPath);
    ParentPath(loaded.projectRoot);
    BProjectError projectError;
    if (!BProject_Load(loaded.manifestPath, &loaded.project, &projectError))
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, projectError.message,
                    loaded.manifestPath);

    if (!BProjectContext_ResolvePath(&loaded, loaded.project.startupWorkspace, loaded.workspacePath,
                                     sizeof(loaded.workspacePath), diagnostics)) {
        return false;
    }

    *destination = loaded;
    return true;
}

bool BProjectContext_ResolvePath(const BProjectContext *context, const char *relativePath,
                                 char *output, size_t outputSize, BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (context == NULL || relativePath == NULL || output == NULL || outputSize == 0) {
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                    "Project context, relative path, and destination are required.", relativePath);
    }

    output[0] = '\0';

    if (context->projectRoot[0] == '\0') {
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Project root is not available.",
                    relativePath);
    }

    if (!BProject_IsPortableRelativePath(relativePath)) {
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                    "Project path must be a portable relative path.", relativePath);
    }

    char candidate[BPROJECT_PATH_MAX];

    int written =
        snprintf(candidate, sizeof(candidate), "%s/%s", context->projectRoot, relativePath);

    if (written < 0 || (size_t)written >= sizeof(candidate)) {
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Project-relative path is too long.",
                    relativePath);
    }

    char resolved[BPROJECT_PATH_MAX];

    if (!FullPath(candidate, resolved)) {
        return Fail(diagnostics, BDIAGNOSTIC_IO, "Could not resolve Project-relative path.",
                    relativePath);
    }

    size_t rootLength = strlen(context->projectRoot);

#ifdef _WIN32
    bool contained = _strnicmp(context->projectRoot, resolved, rootLength) == 0;
#else
    bool contained = strncmp(context->projectRoot, resolved, rootLength) == 0;
#endif

    if (!contained || (resolved[rootLength] != '/' && resolved[rootLength] != '\\')) {
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                    "Project-relative path escapes the Project root.", relativePath);
    }

    if (strlen(resolved) >= outputSize) {
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                    "Resolved Project-relative path is too long.", relativePath);
    }

    snprintf(output, outputSize, "%s", resolved);

    return true;
}

bool BProjectContext_Discover(int argumentCount, char **arguments, BProjectContext *destination,
                              BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);
    if (destination == NULL || argumentCount < 1 || arguments == NULL)
        return Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                    "Process arguments and destination context are required.", NULL);

    for (int i = 1; i < argumentCount; ++i) {
        if (strcmp(arguments[i], "--project") == 0) {
            if (i + 1 >= argumentCount)
                return Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                            "--project requires a manifest path.", NULL);
            return BProjectContext_Load(arguments[i + 1], destination, diagnostics);
        }
    }

    char directory[BPROJECT_PATH_MAX];
    char manifest[BPROJECT_PATH_MAX];
    if (GET_CWD(directory, sizeof(directory)) != NULL &&
        FindManifest(directory, manifest, diagnostics))
        return BProjectContext_Load(manifest, destination, diagnostics);
    if (BDiagnosticList_FirstError(diagnostics) != NULL)
        return false;

    if (!FullPath(arguments[0], directory))
        return Fail(diagnostics, BDIAGNOSTIC_IO,
                    "Could not resolve the executable path for Project discovery.", arguments[0]);
    ParentPath(directory);
    for (;;) {
        if (FindManifest(directory, manifest, diagnostics))
            return BProjectContext_Load(manifest, destination, diagnostics);
        if (BDiagnosticList_FirstError(diagnostics) != NULL)
            return false;
        char previous[BPROJECT_PATH_MAX];
        snprintf(previous, sizeof(previous), "%s", directory);
        ParentPath(directory);
        if (strcmp(previous, directory) == 0)
            break;
    }
    return Fail(diagnostics, BDIAGNOSTIC_IO, "No .basilproject manifest could be discovered.",
                NULL);
}
