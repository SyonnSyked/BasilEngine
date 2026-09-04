#ifndef BASIL_ENGINE_ASSET_REF_H
#define BASIL_ENGINE_ASSET_REF_H

#include "../core/BDiagnostic.h"
#include "BProject.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BASSET_ID_MAX 64

typedef struct BAssetRef {
    char id[BASSET_ID_MAX];
    char path[BPROJECT_PATH_MAX];
} BAssetRef;

/*
 * Asset references use the stable asset ID as authoritative identity.
 * The Project-relative path is a readable location hint.
 *
 * An empty reference is represented by both fields being empty.
 * A non-empty reference is valid only when both fields are present
 * and the path is a portable Project-relative asset path.
 */
void BAssetRef_Clear(BAssetRef *reference);

bool BAssetRef_IsEmpty(const BAssetRef *reference);

bool BAssetRef_Validate(const BAssetRef *reference, BDiagnosticList *diagnostics);

bool BAssetRef_Set(BAssetRef *reference, const char *id, const char *path,
                   BDiagnosticList *diagnostics);

#ifdef __cplusplus
}
#endif

#endif
