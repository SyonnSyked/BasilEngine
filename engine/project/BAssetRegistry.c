#include "BAssetRegistry.h"

#include "../../thirdparty/cjson/cJSON.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static bool BAssetRegistry_Fail(BDiagnosticList *diagnostics, BDiagnosticCode code,
                                const char *message, const char *path)
{
    BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, code, message, path);

    return false;
}

static bool BAssetRegistry_Reserve(BAssetRegistry *registry, size_t required,
                                   BDiagnosticList *diagnostics)
{
    if (required <= registry->capacity)
        return true;

    if (required > BASSET_RECORD_MAX) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                   "Asset registry contains too many records.", NULL);
    }

    size_t capacity = registry->capacity == 0 ? 16 : registry->capacity;

    while (capacity < required) {
        if (capacity >= BASSET_RECORD_MAX / 2) {
            capacity = BASSET_RECORD_MAX;
            break;
        }

        capacity *= 2;
    }

    BAssetRecord *records =
        (BAssetRecord *)realloc(registry->records, capacity * sizeof(BAssetRecord));

    if (records == NULL) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                   "Out of memory while growing the asset registry.", NULL);
    }

    registry->records = records;
    registry->capacity = capacity;

    return true;
}

static char *BAssetRegistry_ReadFile(const char *path, BDiagnosticList *diagnostics)
{
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_IO, "Could not open the asset registry.",
                            path);

        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);

        BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_IO, "Could not measure the asset registry.",
                            path);

        return NULL;
    }

    long length = ftell(file);

    if (length < 0 || (unsigned long long)length > BASSET_REGISTRY_FILE_MAX ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);

        BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                            "Asset registry is too large or could not be read.", path);

        return NULL;
    }

    size_t fileLength = (size_t)length;

    char *contents = (char *)calloc(fileLength + 1, 1);

    if (contents == NULL) {
        fclose(file);

        BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                            "Out of memory while reading the asset registry.", path);

        return NULL;
    }

    size_t bytesRead = fread(contents, 1, fileLength, file);

    bool closed = fclose(file) == 0;

    if (bytesRead != fileLength || !closed) {
        free(contents);

        BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_IO,
                            "Could not read the complete asset registry.", path);

        return NULL;
    }

    return contents;
}

static bool BAssetRegistry_ParseHash(const char *value, uint64_t *output)
{
    if (value == NULL || output == NULL || value[0] == '\0') {
        return false;
    }

    errno = 0;

    char *end = NULL;
    unsigned long long parsed = strtoull(value, &end, 16);

    if (errno != 0 || end == value || *end != '\0') {
        return false;
    }

    *output = (uint64_t)parsed;
    return true;
}

static bool BAssetRegistry_IdInUse(const BAssetRegistry *previous, const BAssetRegistry *assigned,
                                   const char *id)
{
    return BAssetRegistry_FindById(previous, id) != NULL ||
           BAssetRegistry_FindById(assigned, id) != NULL;
}

static bool BAssetRegistry_MakeId(const BAssetRegistry *previous, const BAssetRegistry *assigned,
                                  const char *path, char *output, size_t outputSize,
                                  BDiagnosticList *diagnostics)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    for (const unsigned char *cursor = (const unsigned char *)path; *cursor != '\0'; ++cursor) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }

    char candidate[BASSET_ID_MAX];

    for (unsigned int suffix = 0;; ++suffix) {
        int written;

        if (suffix == 0) {
            written =
                snprintf(candidate, sizeof(candidate), "asset-%016llx", (unsigned long long)hash);
        } else {
            written = snprintf(candidate, sizeof(candidate), "asset-%016llx-%u",
                               (unsigned long long)hash, suffix);
        }

        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Could not generate a stable asset ID.", path);
        }

        if (!BAssetRegistry_IdInUse(previous, assigned, candidate)) {
            snprintf(output, outputSize, "%s", candidate);

            return true;
        }

        if (suffix == UINT_MAX) {
            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Could not find an unused stable asset ID.", path);
        }
    }
}

static int BAssetRegistry_ComparePath(const void *left, const void *right)
{
    const BAssetRecord *a = (const BAssetRecord *)left;

    const BAssetRecord *b = (const BAssetRecord *)right;

    return strcmp(a->path, b->path);
}

void BAssetRegistry_Init(BAssetRegistry *registry)
{
    if (registry == NULL)
        return;

    memset(registry, 0, sizeof(*registry));
    registry->schemaVersion = BASSET_REGISTRY_SCHEMA_VERSION;
}

void BAssetRegistry_Destroy(BAssetRegistry *registry)
{
    if (registry == NULL)
        return;

    free(registry->records);
    memset(registry, 0, sizeof(*registry));
}

void BAssetRegistry_Swap(BAssetRegistry *left, BAssetRegistry *right)
{
    if (left == NULL || right == NULL || left == right) {
        return;
    }

    BAssetRegistry temporary = *left;
    *left = *right;
    *right = temporary;
}

bool BAssetRegistry_Assign(BAssetRegistry *destination, const BAssetRecord *records, size_t count,
                           BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (destination == NULL || (count > 0 && records == NULL)) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                   "Asset registry destination and records are required.", NULL);
    }

    if (count > BASSET_RECORD_MAX) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                   "Asset registry contains too many records.", NULL);
    }

    BAssetRegistry assigned;
    BAssetRegistry_Init(&assigned);

    if (!BAssetRegistry_Reserve(&assigned, count, diagnostics)) {
        BAssetRegistry_Destroy(&assigned);
        return false;
    }

    if (count > 0) {
        memcpy(assigned.records, records, count * sizeof(BAssetRecord));

        assigned.count = count;
    }

    if (!BAssetRegistry_Validate(&assigned, diagnostics)) {
        BAssetRegistry_Destroy(&assigned);
        return false;
    }

    BAssetRegistry_Swap(destination, &assigned);

    BAssetRegistry_Destroy(&assigned);

    return true;
}

bool BAssetRegistry_Reconcile(const BAssetRegistry *previous, const BAssetObservation *observations,
                              size_t observationCount, BAssetRegistry *destination,
                              BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (previous == NULL || destination == NULL || (observationCount > 0 && observations == NULL)) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                   "Previous registry, observations, and destination are required.",
                                   NULL);
    }

    if (observationCount > BASSET_RECORD_MAX) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                   "Too many assets were observed.", NULL);
    }

    if (!BAssetRegistry_Validate(previous, diagnostics)) {
        return false;
    }

    bool *previousUsed = NULL;

    if (previous->count > 0) {
        previousUsed = (bool *)calloc(previous->count, sizeof(bool));

        if (previousUsed == NULL) {
            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                       "Out of memory while reconciling asset identities.", NULL);
        }
    }

    BAssetRegistry reconciled;
    BAssetRegistry_Init(&reconciled);

    if (!BAssetRegistry_Reserve(&reconciled, observationCount, diagnostics)) {
        free(previousUsed);
        BAssetRegistry_Destroy(&reconciled);
        return false;
    }

    for (size_t i = 0; i < observationCount; ++i) {
        const BAssetObservation *observation = &observations[i];

        if (observation->path[0] == '\0' || strlen(observation->path) >= BPROJECT_PATH_MAX) {
            free(previousUsed);
            BAssetRegistry_Destroy(&reconciled);

            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Observed asset has an invalid path.", observation->path);
        }

        BAssetRecord record;
        memset(&record, 0, sizeof(record));

        snprintf(record.path, sizeof(record.path), "%s", observation->path);

        record.kind = observation->kind;

        record.size = observation->size;

        record.contentHash = observation->contentHash;

        /*
         * First priority:
         * same Project-relative path.
         *
         * Content may have changed, but identity
         * remains the same asset.
         */
        size_t pathMatch = previous->count;

        for (size_t oldIndex = 0; oldIndex < previous->count; ++oldIndex) {
            if (strcmp(previous->records[oldIndex].path, observation->path) == 0) {
                pathMatch = oldIndex;
                break;
            }
        }

        if (pathMatch != previous->count) {
            snprintf(record.id, sizeof(record.id), "%s", previous->records[pathMatch].id);

            previousUsed[pathMatch] = true;
        } else {
            /*
             * Second priority:
             * exactly one unmatched prior asset has
             * the same size and content hash.
             *
             * That is our conservative move detector.
             */
            size_t contentMatch = previous->count;

            bool ambiguous = false;

            for (size_t oldIndex = 0; oldIndex < previous->count; ++oldIndex) {
                if (previousUsed[oldIndex])
                    continue;

                const BAssetRecord *old = &previous->records[oldIndex];

                if (old->size == observation->size &&
                    old->contentHash == observation->contentHash) {
                    if (contentMatch != previous->count) {
                        ambiguous = true;
                        break;
                    }

                    contentMatch = oldIndex;
                }
            }

            if (!ambiguous && contentMatch != previous->count) {
                snprintf(record.id, sizeof(record.id), "%s", previous->records[contentMatch].id);

                previousUsed[contentMatch] = true;
            } else {
                /*
                 * New or ambiguous asset:
                 * never guess.
                 */
                if (!BAssetRegistry_MakeId(previous, &reconciled, observation->path, record.id,
                                           sizeof(record.id), diagnostics)) {
                    free(previousUsed);
                    BAssetRegistry_Destroy(&reconciled);

                    return false;
                }
            }
        }

        reconciled.records[reconciled.count++] = record;
    }

    free(previousUsed);

    if (reconciled.count > 1) {
        qsort(reconciled.records, reconciled.count, sizeof(BAssetRecord),
              BAssetRegistry_ComparePath);
    }

    if (!BAssetRegistry_Validate(&reconciled, diagnostics)) {
        BAssetRegistry_Destroy(&reconciled);

        return false;
    }

    BAssetRegistry_Swap(destination, &reconciled);

    BAssetRegistry_Destroy(&reconciled);

    return true;
}

const char *BAssetKind_ToString(BAssetKind kind)
{
    switch (kind) {
        case BASSET_KIND_TEXT_SPRITE:
            return "text-sprite";

        case BASSET_KIND_DATA:
            return "data";

        case BASSET_KIND_FONT:
            return "font";

        case BASSET_KIND_AUDIO:
            return "audio";

        default:
            return "unknown";
    }
}

bool BAssetKind_FromString(const char *value, BAssetKind *outKind)
{
    if (value == NULL || outKind == NULL) {
        return false;
    }

    if (strcmp(value, "text-sprite") == 0)
        *outKind = BASSET_KIND_TEXT_SPRITE;
    else if (strcmp(value, "data") == 0)
        *outKind = BASSET_KIND_DATA;
    else if (strcmp(value, "font") == 0)
        *outKind = BASSET_KIND_FONT;
    else if (strcmp(value, "audio") == 0)
        *outKind = BASSET_KIND_AUDIO;
    else
        return false;

    return true;
}

bool BAssetRegistry_Validate(const BAssetRegistry *registry, BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (registry == NULL) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                   "Asset registry was null.", NULL);
    }

    if (registry->schemaVersion != BASSET_REGISTRY_SCHEMA_VERSION) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_UNSUPPORTED_VERSION,
                                   "Unsupported asset registry schema version.", NULL);
    }

    if (registry->count > BASSET_RECORD_MAX || registry->count > registry->capacity ||
        (registry->count > 0 && registry->records == NULL)) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                   "Asset registry storage is invalid.", NULL);
    }

    for (size_t i = 0; i < registry->count; ++i) {
        const BAssetRecord *record = &registry->records[i];

        if (record->id[0] == '\0' || strlen(record->id) >= BASSET_ID_MAX) {
            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Asset registry contains an invalid asset ID.",
                                       record->path);
        }

        if (record->path[0] == '\0' || strlen(record->path) >= BPROJECT_PATH_MAX ||
            !BProject_IsPortableRelativePath(record->path)) {
            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Asset registry contains an invalid asset path.",
                                       record->path);
        }

        if (strncmp(record->path, "assets/", strlen("assets/")) != 0) {
            return BAssetRegistry_Fail(
                diagnostics, BDIAGNOSTIC_INVALID_DATA,
                "Asset registry path must be inside the Project assets directory.", record->path);
        }

        if (record->kind < BASSET_KIND_TEXT_SPRITE || record->kind > BASSET_KIND_AUDIO) {
            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Asset registry contains an invalid asset kind.",
                                       record->path);
        }

        if (record->size > BASSET_FILE_MAX) {
            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Asset registry contains an oversized asset.", record->path);
        }

        for (size_t other = i + 1; other < registry->count; ++other) {
            const BAssetRecord *comparison = &registry->records[other];

            if (strcmp(record->id, comparison->id) == 0) {
                return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "Asset registry contains a duplicate asset ID.",
                                           record->path);
            }

            if (strcmp(record->path, comparison->path) == 0) {
                return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "Asset registry contains a duplicate asset path.",
                                           record->path);
            }
        }
    }

    return true;
}

bool BAssetRegistry_Load(const char *registryPath, BAssetRegistry *destination,
                         BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (registryPath == NULL || destination == NULL) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                   "Asset registry path and destination are required.",
                                   registryPath);
    }

    char *contents = BAssetRegistry_ReadFile(registryPath, diagnostics);

    if (contents == NULL)
        return false;

    cJSON *root = cJSON_Parse(contents);

    free(contents);

    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);

        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                   "Asset registry contains invalid JSON.", registryPath);
    }

    cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");

    cJSON *assets = cJSON_GetObjectItemCaseSensitive(root, "assets");

    if (!cJSON_IsNumber(schema) || !cJSON_IsArray(assets) ||
        schema->valuedouble != (double)schema->valueint) {
        cJSON_Delete(root);

        return BAssetRegistry_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_DATA,
            "Asset registry is missing required fields or contains invalid field types.",
            registryPath);
    }

    if (schema->valueint != BASSET_REGISTRY_SCHEMA_VERSION) {
        cJSON_Delete(root);

        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_UNSUPPORTED_VERSION,
                                   "Unsupported asset registry schema version.", registryPath);
    }

    int assetCount = cJSON_GetArraySize(assets);

    if (assetCount < 0 || assetCount > BASSET_RECORD_MAX) {
        cJSON_Delete(root);

        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                   "Asset registry contains too many records.", registryPath);
    }

    BAssetRegistry loaded;
    BAssetRegistry_Init(&loaded);

    if (!BAssetRegistry_Reserve(&loaded, (size_t)assetCount, diagnostics)) {
        cJSON_Delete(root);
        BAssetRegistry_Destroy(&loaded);
        return false;
    }

    for (int i = 0; i < assetCount; ++i) {
        cJSON *item = cJSON_GetArrayItem(assets, i);

        cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");

        cJSON *path = cJSON_GetObjectItemCaseSensitive(item, "path");

        cJSON *type = cJSON_GetObjectItemCaseSensitive(item, "type");

        cJSON *size = cJSON_GetObjectItemCaseSensitive(item, "size");

        cJSON *hash = cJSON_GetObjectItemCaseSensitive(item, "hash");

        BAssetRecord record;
        memset(&record, 0, sizeof(record));

        if (!cJSON_IsObject(item) || !cJSON_IsString(id) || !cJSON_IsString(path) ||
            !cJSON_IsString(type) || !cJSON_IsNumber(size) || !cJSON_IsString(hash) ||
            strlen(id->valuestring) >= BASSET_ID_MAX ||
            strlen(path->valuestring) >= BPROJECT_PATH_MAX || size->valuedouble < 0.0 ||
            size->valuedouble > (double)BASSET_FILE_MAX ||
            size->valuedouble != (double)(uintmax_t)size->valuedouble ||
            !BAssetKind_FromString(type->valuestring, &record.kind) ||
            !BAssetRegistry_ParseHash(hash->valuestring, &record.contentHash)) {
            cJSON_Delete(root);
            BAssetRegistry_Destroy(&loaded);

            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "Asset registry contains an invalid record.", registryPath);
        }

        snprintf(record.id, sizeof(record.id), "%s", id->valuestring);

        snprintf(record.path, sizeof(record.path), "%s", path->valuestring);

        record.size = (uintmax_t)size->valuedouble;

        loaded.records[loaded.count++] = record;
    }

    cJSON_Delete(root);

    if (!BAssetRegistry_Validate(&loaded, diagnostics)) {
        BAssetRegistry_Destroy(&loaded);
        return false;
    }

    BAssetRegistry_Swap(destination, &loaded);

    BAssetRegistry_Destroy(&loaded);

    return true;
}

static bool BAssetRegistry_WriteJson(const BAssetRegistry *registry, const char *path,
                                     BDiagnosticList *diagnostics)
{
    cJSON *root = cJSON_CreateObject();

    cJSON *assets = cJSON_CreateArray();

    if (root == NULL || assets == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(assets);

        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                   "Out of memory while creating the asset registry.", path);
    }

    cJSON_AddNumberToObject(root, "schemaVersion", registry->schemaVersion);

    cJSON_AddItemToObject(root, "assets", assets);

    for (size_t i = 0; i < registry->count; ++i) {
        const BAssetRecord *record = &registry->records[i];

        cJSON *item = cJSON_CreateObject();

        if (item == NULL) {
            cJSON_Delete(root);

            return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                       "Out of memory while serializing an asset record.", path);
        }

        char hash[17];

        snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)record->contentHash);

        cJSON_AddStringToObject(item, "id", record->id);

        cJSON_AddStringToObject(item, "path", record->path);

        cJSON_AddStringToObject(item, "type", BAssetKind_ToString(record->kind));

        cJSON_AddNumberToObject(item, "size", (double)record->size);

        cJSON_AddStringToObject(item, "hash", hash);

        cJSON_AddItemToArray(assets, item);
    }

    char *json = cJSON_Print(root);

    cJSON_Delete(root);

    if (json == NULL) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_IO,
                                   "Could not serialize the asset registry.", path);
    }

    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        cJSON_free(json);

        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_IO,
                                   "Could not create the asset registry file.", path);
    }

    size_t length = strlen(json);

    bool written = fwrite(json, 1, length, file) == length && fwrite("\n", 1, 1, file) == 1;

    bool closed = fclose(file) == 0;

    cJSON_free(json);

    if (!written || !closed) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_IO,
                                   "Could not write the complete asset registry.", path);
    }

    return true;
}

bool BAssetRegistry_Save(const BAssetRegistry *registry, const char *registryPath,
                         BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (registryPath == NULL) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                   "Asset registry path is required.", NULL);
    }

    if (!BAssetRegistry_Validate(registry, diagnostics)) {
        return false;
    }

    char temporary[BDIAGNOSTIC_PATH_MAX + 8];

    char backup[BDIAGNOSTIC_PATH_MAX + 8];

    int temporaryLength = snprintf(temporary, sizeof(temporary), "%s.tmp", registryPath);

    int backupLength = snprintf(backup, sizeof(backup), "%s.bak", registryPath);

    if (temporaryLength < 0 || (size_t)temporaryLength >= sizeof(temporary) || backupLength < 0 ||
        (size_t)backupLength >= sizeof(backup)) {
        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                   "Asset registry path is too long.", registryPath);
    }

    remove(temporary);

    if (!BAssetRegistry_WriteJson(registry, temporary, diagnostics)) {
        return false;
    }

    FILE *existing = fopen(registryPath, "rb");

    bool destinationExists = existing != NULL;

    if (existing != NULL)
        fclose(existing);

    if (destinationExists) {
        remove(backup);

        if (rename(registryPath, backup) != 0) {
            remove(temporary);

            return BAssetRegistry_Fail(
                diagnostics, BDIAGNOSTIC_IO,
                "Could not back up the existing asset registry before saving.", registryPath);
        }
    }

    if (rename(temporary, registryPath) != 0) {
        if (destinationExists) {
            rename(backup, registryPath);
        }

        remove(temporary);

        return BAssetRegistry_Fail(diagnostics, BDIAGNOSTIC_IO,
                                   "Could not replace the asset registry.", registryPath);
    }

    return true;
}

const BAssetRecord *BAssetRegistry_FindById(const BAssetRegistry *registry, const char *id)
{
    if (registry == NULL || id == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < registry->count; ++i) {
        if (strcmp(registry->records[i].id, id) == 0) {
            return &registry->records[i];
        }
    }

    return NULL;
}

const BAssetRecord *BAssetRegistry_FindByPath(const BAssetRegistry *registry, const char *path)
{
    if (registry == NULL || path == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < registry->count; ++i) {
        if (strcmp(registry->records[i].path, path) == 0) {
            return &registry->records[i];
        }
    }

    return NULL;
}

const BAssetRecord *BAssetRegistry_ResolveRef(const BAssetRegistry *registry,
                                              const BAssetRef *reference,
                                              BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (registry == NULL || reference == NULL) {
        BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, BDIAGNOSTIC_INVALID_ARGUMENT,
                            "Asset registry and reference are required.", NULL);

        return NULL;
    }

    if (!BAssetRef_Validate(reference, diagnostics)) {
        return NULL;
    }

    const BAssetRecord *record = BAssetRegistry_FindById(registry, reference->id);

    if (record == NULL) {
        BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, BDIAGNOSTIC_INVALID_DATA,
                            "Asset reference stable ID does not exist in the Project registry.",
                            reference->path);

        return NULL;
    }

    return record;
}

bool BAssetRegistry_RefreshRefPath(const BAssetRegistry *registry, BAssetRef *reference,
                                   BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);

    if (reference == NULL) {
        BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, BDIAGNOSTIC_INVALID_ARGUMENT,
                            "Asset reference is required.", NULL);

        return false;
    }

    const BAssetRecord *record = BAssetRegistry_ResolveRef(registry, reference, diagnostics);

    if (record == NULL)
        return false;

    snprintf(reference->path, sizeof(reference->path), "%s", record->path);

    return true;
}
