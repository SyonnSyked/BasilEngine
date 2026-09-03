#ifndef BASIL_ENGINE_ASSET_REGISTRY_H
#define BASIL_ENGINE_ASSET_REGISTRY_H

#include "../core/BDiagnostic.h"
#include "BProject.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BASSET_REGISTRY_SCHEMA_VERSION 1
#define BASSET_ID_MAX 64
#define BASSET_RECORD_MAX 8192
#define BASSET_REGISTRY_FILE_MAX (16u * 1024u * 1024u)
#define BASSET_FILE_MAX (256u * 1024u * 1024u)

typedef enum BAssetKind {
    BASSET_KIND_TEXT_SPRITE,
    BASSET_KIND_DATA,
    BASSET_KIND_FONT,
    BASSET_KIND_AUDIO
} BAssetKind;

typedef struct BAssetRecord {
    char id[BASSET_ID_MAX];
    char path[BPROJECT_PATH_MAX];
    BAssetKind kind;
    uintmax_t size;
    uint64_t contentHash;
} BAssetRecord;

typedef struct BAssetObservation {
    char path[BPROJECT_PATH_MAX];
    BAssetKind kind;
    uintmax_t size;
    uint64_t contentHash;
} BAssetObservation;

typedef struct BAssetRegistry {
    int schemaVersion;
    size_t count;
    size_t capacity;
    BAssetRecord *records;
} BAssetRegistry;

/*
 * Registries own their record storage.
 * Initialize before first use and destroy when finished.
 * Load replaces an initialized destination only after success.
 */
void BAssetRegistry_Init(BAssetRegistry *registry);
void BAssetRegistry_Destroy(BAssetRegistry *registry);
void BAssetRegistry_Swap(BAssetRegistry *left, BAssetRegistry *right);

bool BAssetRegistry_Assign(BAssetRegistry *destination, const BAssetRecord *records, size_t count,
                           BDiagnosticList *diagnostics);

bool BAssetRegistry_Reconcile(const BAssetRegistry *previous, const BAssetObservation *observations,
                              size_t observationCount, BAssetRegistry *destination,
                              BDiagnosticList *diagnostics);

bool BAssetRegistry_Validate(const BAssetRegistry *registry, BDiagnosticList *diagnostics);

bool BAssetRegistry_Load(const char *registryPath, BAssetRegistry *destination,
                         BDiagnosticList *diagnostics);

bool BAssetRegistry_Save(const BAssetRegistry *registry, const char *registryPath,
                         BDiagnosticList *diagnostics);

const BAssetRecord *BAssetRegistry_FindById(const BAssetRegistry *registry, const char *id);

const BAssetRecord *BAssetRegistry_FindByPath(const BAssetRegistry *registry, const char *path);

const char *BAssetKind_ToString(BAssetKind kind);

bool BAssetKind_FromString(const char *value, BAssetKind *outKind);

#ifdef __cplusplus
}
#endif

#endif
