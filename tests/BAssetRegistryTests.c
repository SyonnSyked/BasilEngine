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

    BAssetRecord identityRecords[2] = {0};

    snprintf(identityRecords[0].id, sizeof(identityRecords[0].id), "%s", "asset-aaaaaaaaaaaaaaaa");

    snprintf(identityRecords[0].path, sizeof(identityRecords[0].path), "%s", "assets/hero.txt");

    identityRecords[0].kind = BASSET_KIND_TEXT_SPRITE;

    identityRecords[0].size = 4;

    identityRecords[0].contentHash = UINT64_C(0x1111111111111111);

    snprintf(identityRecords[1].id, sizeof(identityRecords[1].id), "%s", "asset-bbbbbbbbbbbbbbbb");

    snprintf(identityRecords[1].path, sizeof(identityRecords[1].path), "%s", "assets/data.json");

    identityRecords[1].kind = BASSET_KIND_DATA;

    identityRecords[1].size = 2;

    identityRecords[1].contentHash = UINT64_C(0x2222222222222222);

    BAssetRegistry identityPrevious;
    BAssetRegistry_Init(&identityPrevious);

    failures += Check(BAssetRegistry_Assign(&identityPrevious, identityRecords, 2, &diagnostics),
                      "identity test registry is assigned");

    BAssetObservation observations[3] = {0};

    snprintf(observations[0].path, sizeof(observations[0].path), "%s", "assets/player.txt");

    observations[0].kind = BASSET_KIND_TEXT_SPRITE;

    observations[0].size = 4;

    observations[0].contentHash = UINT64_C(0x1111111111111111);

    snprintf(observations[1].path, sizeof(observations[1].path), "%s", "assets/data.json");

    observations[1].kind = BASSET_KIND_DATA;

    observations[1].size = 3;

    observations[1].contentHash = UINT64_C(0x3333333333333333);

    snprintf(observations[2].path, sizeof(observations[2].path), "%s", "assets/sound.ogg");

    observations[2].kind = BASSET_KIND_AUDIO;

    observations[2].size = 8;

    observations[2].contentHash = UINT64_C(0x4444444444444444);

    BAssetRegistry reconciled;
    BAssetRegistry_Init(&reconciled);

    failures += Check(
        BAssetRegistry_Reconcile(&identityPrevious, observations, 3, &reconciled, &diagnostics),
        "asset observations reconcile");

    const BAssetRecord *movedHero = BAssetRegistry_FindByPath(&reconciled, "assets/player.txt");

    failures += Check(movedHero != NULL && strcmp(movedHero->id, "asset-aaaaaaaaaaaaaaaa") == 0,
                      "unique content move preserves stable ID");

    const BAssetRecord *changedData = BAssetRegistry_FindByPath(&reconciled, "assets/data.json");

    failures += Check(changedData != NULL && strcmp(changedData->id, "asset-bbbbbbbbbbbbbbbb") == 0,
                      "same path preserves identity after content changes");

    const BAssetRecord *newSound = BAssetRegistry_FindByPath(&reconciled, "assets/sound.ogg");

    failures += Check(newSound != NULL && newSound->id[0] != '\0' &&
                          strcmp(newSound->id, "asset-aaaaaaaaaaaaaaaa") != 0 &&
                          strcmp(newSound->id, "asset-bbbbbbbbbbbbbbbb") != 0,
                      "new asset receives a new stable ID");

    BAssetRecord ambiguousRecords[2] = {0};

    snprintf(ambiguousRecords[0].id, sizeof(ambiguousRecords[0].id), "%s", "asset-first");

    snprintf(ambiguousRecords[0].path, sizeof(ambiguousRecords[0].path), "%s", "assets/first.txt");

    ambiguousRecords[0].kind = BASSET_KIND_TEXT_SPRITE;

    ambiguousRecords[0].size = 5;
    ambiguousRecords[0].contentHash = UINT64_C(0x5555555555555555);

    snprintf(ambiguousRecords[1].id, sizeof(ambiguousRecords[1].id), "%s", "asset-second");

    snprintf(ambiguousRecords[1].path, sizeof(ambiguousRecords[1].path), "%s", "assets/second.txt");

    ambiguousRecords[1].kind = BASSET_KIND_TEXT_SPRITE;

    ambiguousRecords[1].size = 5;
    ambiguousRecords[1].contentHash = UINT64_C(0x5555555555555555);

    BAssetRegistry ambiguousPrevious;
    BAssetRegistry_Init(&ambiguousPrevious);

    failures += Check(BAssetRegistry_Assign(&ambiguousPrevious, ambiguousRecords, 2, &diagnostics),
                      "ambiguous registry is assigned");

    BAssetObservation ambiguousObservation = {0};

    snprintf(ambiguousObservation.path, sizeof(ambiguousObservation.path), "%s",
             "assets/moved.txt");

    ambiguousObservation.kind = BASSET_KIND_TEXT_SPRITE;

    ambiguousObservation.size = 5;

    ambiguousObservation.contentHash = UINT64_C(0x5555555555555555);

    BAssetRegistry ambiguousResult;
    BAssetRegistry_Init(&ambiguousResult);

    failures += Check(BAssetRegistry_Reconcile(&ambiguousPrevious, &ambiguousObservation, 1,
                                               &ambiguousResult, &diagnostics),
                      "ambiguous observation reconciles safely");

    const BAssetRecord *ambiguousAsset =
        BAssetRegistry_FindByPath(&ambiguousResult, "assets/moved.txt");

    failures += Check(ambiguousAsset != NULL && strcmp(ambiguousAsset->id, "asset-first") != 0 &&
                          strcmp(ambiguousAsset->id, "asset-second") != 0,
                      "ambiguous content does not guess identity");

    remove(savedPath);
    remove(backupPath);

    BAssetRegistry_Destroy(&roundTrip);
    BAssetRegistry_Destroy(&registry);

    BAssetRegistry_Destroy(&registry);

    BAssetRegistry_Destroy(&ambiguousResult);
    BAssetRegistry_Destroy(&ambiguousPrevious);
    BAssetRegistry_Destroy(&reconciled);
    BAssetRegistry_Destroy(&identityPrevious);

    failures += Check(registry.records == NULL && registry.count == 0, "destroy is idempotent");

    if (failures == 0)
        printf("BAssetRegistryTests passed.\n");

    return failures == 0 ? 0 : 1;
}
