#include "../engine/project/BAssetRegistry.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define GET_PROCESS_ID() _getpid()
#else
#include <unistd.h>
#define GET_PROCESS_ID() getpid()
#endif

static int Check(bool condition, const char *message)
{
    if (condition)
        return 0;

    fprintf(stderr, "FAILED: %s\n", message);

    return 1;
}

static bool FileExists(const char *path)
{
    struct stat information;
    return stat(path, &information) == 0;
}

static BDiagnosticCode FirstErrorCode(const BDiagnosticList *diagnostics)
{
    const BDiagnostic *diagnostic = BDiagnosticList_FirstError(diagnostics);

    return diagnostic != NULL ? diagnostic->code : BDIAGNOSTIC_NONE;
}

int main(void)
{
    int failures = 0;

    BDiagnosticList diagnostics;

    BAssetRegistry registry;
    BAssetRegistry_Init(&registry);

    failures += Check(registry.schemaVersion == BASSET_REGISTRY_SCHEMA_VERSION,
                      "initialized registry uses current schema");

    failures +=
        Check(registry.count == 0 && registry.records == NULL, "initialized registry starts empty");

    failures += Check(BAssetRegistry_Validate(&registry, &diagnostics), "empty registry validates");

    failures += Check(
        BAssetRegistry_Load("test-fixtures/asset-registry-valid.json", &registry, &diagnostics),
        "valid asset registry loads");

    failures += Check(registry.count == 2, "valid registry loads both records");

    const BAssetRecord *arena = BAssetRegistry_FindById(&registry, "asset-4cd22aaa66b511e0");

    failures += Check(arena != NULL && strcmp(arena->path, "assets/arena.txt") == 0 &&
                          arena->kind == BASSET_KIND_TEXT_SPRITE && arena->size == 1181 &&
                          arena->contentHash == UINT64_C(0x20edd3441ced4e03),
                      "asset can be found by stable ID");

    const BAssetRecord *player = BAssetRegistry_FindByPath(&registry, "assets/player.txt");

    failures += Check(player != NULL && strcmp(player->id, "asset-37c7d02565c20340") == 0,
                      "asset can be found by Project-relative path");

    char savedPath[BPROJECT_PATH_MAX];

    snprintf(savedPath, sizeof(savedPath), "AssetRegistry_%ld_%d.json", (long)time(NULL),
             (int)GET_PROCESS_ID());

    failures +=
        Check(BAssetRegistry_Save(&registry, savedPath, &diagnostics), "asset registry saves");

    failures += Check(FileExists(savedPath), "saved asset registry exists");

    failures += Check(BAssetRegistry_Save(&registry, savedPath, &diagnostics),
                      "existing asset registry saves safely");

    char backupPath[BPROJECT_PATH_MAX + 8];

    snprintf(backupPath, sizeof(backupPath), "%s.bak", savedPath);

    failures += Check(FileExists(backupPath), "replacement save preserves a backup");

    BAssetRegistry roundTrip;
    BAssetRegistry_Init(&roundTrip);

    failures += Check(BAssetRegistry_Load(savedPath, &roundTrip, &diagnostics),
                      "saved asset registry reloads");

    failures += Check(roundTrip.count == 2 &&
                          BAssetRegistry_FindById(&roundTrip, "asset-4cd22aaa66b511e0") != NULL &&
                          BAssetRegistry_FindById(&roundTrip, "asset-37c7d02565c20340") != NULL,
                      "stable IDs survive save-load round trip");

    failures += Check(!BAssetRegistry_Load("test-fixtures/asset-registry-duplicate-id.json",
                                           &roundTrip, &diagnostics) &&
                          FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
                      "duplicate asset IDs are rejected");

    failures += Check(roundTrip.count == 2 &&
                          BAssetRegistry_FindById(&roundTrip, "asset-4cd22aaa66b511e0") != NULL,
                      "failed duplicate-ID load preserves destination");

    failures += Check(!BAssetRegistry_Load("test-fixtures/asset-registry-duplicate-path.json",
                                           &roundTrip, &diagnostics) &&
                          FirstErrorCode(&diagnostics) == BDIAGNOSTIC_INVALID_DATA,
                      "duplicate asset paths are rejected");

    failures += Check(roundTrip.count == 2 &&
                          BAssetRegistry_FindByPath(&roundTrip, "assets/player.txt") != NULL,
                      "failed duplicate-path load preserves destination");

    failures += Check(!BAssetRegistry_Load("test-fixtures/does-not-exist-assets.json", &roundTrip,
                                           &diagnostics) &&
                          FirstErrorCode(&diagnostics) == BDIAGNOSTIC_IO,
                      "missing registry reports an IO diagnostic");

    failures += Check(roundTrip.count == 2, "missing registry load preserves destination");

    BAssetRecord assignedRecords[2] = {0};

    snprintf(assignedRecords[0].id, sizeof(assignedRecords[0].id), "%s", "asset-aaaaaaaaaaaaaaaa");

    snprintf(assignedRecords[0].path, sizeof(assignedRecords[0].path), "%s", "assets/a.txt");

    assignedRecords[0].kind = BASSET_KIND_TEXT_SPRITE;

    assignedRecords[0].size = 4;

    assignedRecords[0].contentHash = UINT64_C(0x1111111111111111);

    snprintf(assignedRecords[1].id, sizeof(assignedRecords[1].id), "%s", "asset-bbbbbbbbbbbbbbbb");

    snprintf(assignedRecords[1].path, sizeof(assignedRecords[1].path), "%s", "assets/b.ogg");

    assignedRecords[1].kind = BASSET_KIND_AUDIO;

    assignedRecords[1].size = 8;

    assignedRecords[1].contentHash = UINT64_C(0x2222222222222222);

    failures += Check(BAssetRegistry_Assign(&roundTrip, assignedRecords, 2, &diagnostics),
                      "registry records can be assigned transactionally");

    failures += Check(BAssetRegistry_FindById(&roundTrip, "asset-bbbbbbbbbbbbbbbb") != NULL,
                      "assigned registry contains new records");

    remove(savedPath);
    remove(backupPath);

    BAssetRegistry_Destroy(&roundTrip);
    BAssetRegistry_Destroy(&registry);

    BAssetRegistry_Destroy(&registry);

    failures += Check(registry.records == NULL && registry.count == 0, "destroy is idempotent");

    if (failures == 0)
        printf("BAssetRegistryTests passed.\n");

    return failures == 0 ? 0 : 1;
}
