#include "BAssetRef.h"

#include <stdio.h>
#include <string.h>

static bool BAssetRef_Fail(BDiagnosticList *diagnostics, BDiagnosticCode code, const char *message,
                           const char *path)
{
    BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, code, message, path);

    return false;
}

void BAssetRef_Clear(BAssetRef *reference)
{
    if (reference == NULL)
        return;

    memset(reference, 0, sizeof(*reference));
}

bool BAssetRef_IsEmpty(const BAssetRef *reference)
{
    if (reference == NULL)
        return true;

    return reference->id[0] == '\0' && reference->path[0] == '\0';
}

bool BAssetRef_Validate(const BAssetRef *reference, BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (reference == NULL) {
        return BAssetRef_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                              "Asset reference was null.", NULL);
    }

    if (reference->id[0] == '\0' || strlen(reference->id) >= BASSET_ID_MAX) {
        return BAssetRef_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                              "Asset reference contains an invalid stable ID.", reference->path);
    }

    if (reference->path[0] == '\0' || strlen(reference->path) >= BPROJECT_PATH_MAX ||
        !BProject_IsPortableRelativePath(reference->path)) {
        return BAssetRef_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                              "Asset reference contains an invalid Project-relative path.",
                              reference->path);
    }

    static const char assetPrefix[] = "assets/";

    if (strncmp(reference->path, assetPrefix, sizeof(assetPrefix) - 1) != 0 ||
        reference->path[sizeof(assetPrefix) - 1] == '\0') {
        return BAssetRef_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_DATA,
            "Asset reference path must identify a file inside the Project assets directory.",
            reference->path);
    }

    return true;
}

bool BAssetRef_Set(BAssetRef *reference, const char *id, const char *path,
                   BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (reference == NULL || id == NULL || path == NULL) {
        return BAssetRef_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                              "Asset reference destination, ID, and path are required.", path);
    }

    if (strlen(id) >= BASSET_ID_MAX || strlen(path) >= BPROJECT_PATH_MAX) {
        return BAssetRef_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                              "Asset reference exceeds Project limits.", path);
    }

    BAssetRef assigned;
    BAssetRef_Clear(&assigned);

    snprintf(assigned.id, sizeof(assigned.id), "%s", id);

    snprintf(assigned.path, sizeof(assigned.path), "%s", path);

    if (!BAssetRef_Validate(&assigned, diagnostics)) {
        return false;
    }

    *reference = assigned;

    return true;
}
