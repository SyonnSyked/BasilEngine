#include "BCollision2D.h"

#include <math.h>
#include <stdio.h>

static int Check(bool condition, const char *message)
{
    if (condition) return 0;
    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

static bool MakeDocument(BWorkspaceDocument *document)
{
    BDiagnosticList diagnostics = {0};
    BWorkspaceDocument_Init(document);
    return BWorkspaceDocument_CreateDefault(document, "Collision", "Collision", &diagnostics);
}

static size_t AddEntity(BWorkspaceDocument *document, const char *name, BTransform2D transform,
                        bool collider, bool trigger, bool enabled)
{
    BDiagnosticList diagnostics = {0};
    size_t index = 0;
    if (!BWorkspaceDocument_AddEntity(document, name, &index, &diagnostics) ||
        !BWorkspaceDocument_AddTransform2D(document, index, transform, false, &diagnostics))
        return BCOLLISION_ENTITY_NONE;
    if (collider) {
        BCollider2D value = BCollider2D_Default();
        value.trigger = trigger;
        if (!BWorkspaceDocument_AddCollider2D(document, index, value, false, &diagnostics))
            return BCOLLISION_ENTITY_NONE;
    }
    document->entities[index].enabled = enabled;
    return index;
}

static int TestMovement(void)
{
    int failures = 0;
    BWorkspaceDocument document;
    BTransform2D resolved = {0};
    failures += Check(MakeDocument(&document), "movement document is created");
    size_t mover = AddEntity(&document, "Mover", (BTransform2D){0, 0}, true, false, true);
    failures += Check(BCollision2D_ResolveMovement(&document, mover, 2, 0, &resolved) &&
                          resolved.x == 2 && resolved.y == 0,
                      "clear X movement succeeds and ignores self");
    failures += Check(BCollision2D_ResolveMovement(&document, mover, 0, 3, &resolved) &&
                          resolved.x == 0 && resolved.y == 3,
                      "clear Y movement succeeds");
    failures += Check(BCollision2D_ResolveMovement(&document, mover, 0, 0, &resolved) &&
                          resolved.x == 0 && resolved.y == 0,
                      "zero delta is a valid processed request");
    failures += Check(!BCollision2D_ResolveMovement(&document, mover, NAN, 0, &resolved),
                      "non-finite movement is rejected");
    BWorkspaceDocument_Destroy(&document);

    MakeDocument(&document);
    mover = AddEntity(&document, "Mover", (BTransform2D){0, 0}, true, false, true);
    AddEntity(&document, "X wall", (BTransform2D){1, 0}, true, false, true);
    failures += Check(BCollision2D_ResolveMovement(&document, mover, 1, 0, &resolved) &&
                          resolved.x == 0 && resolved.y == 0,
                      "solid blocks X but the request succeeds");
    BWorkspaceDocument_Destroy(&document);

    MakeDocument(&document);
    mover = AddEntity(&document, "Mover", (BTransform2D){0, 0}, true, false, true);
    AddEntity(&document, "Y wall", (BTransform2D){0, 1}, true, false, true);
    failures += Check(BCollision2D_ResolveMovement(&document, mover, 0, 1, &resolved) &&
                          resolved.x == 0 && resolved.y == 0,
                      "solid blocks Y but the request succeeds");
    BWorkspaceDocument_Destroy(&document);

    MakeDocument(&document);
    mover = AddEntity(&document, "Mover", (BTransform2D){0, 0}, true, false, true);
    AddEntity(&document, "X wall", (BTransform2D){1, 0}, true, false, true);
    failures += Check(BCollision2D_ResolveMovement(&document, mover, 1, 1, &resolved) &&
                          resolved.x == 0 && resolved.y == 1,
                      "diagonal movement slides on the clear axis");
    BWorkspaceDocument_Destroy(&document);

    MakeDocument(&document);
    mover = AddEntity(&document, "Mover", (BTransform2D){0, 0}, true, false, true);
    AddEntity(&document, "Trigger", (BTransform2D){1, 0}, true, true, true);
    AddEntity(&document, "Disabled wall", (BTransform2D){1, 0}, true, false, false);
    failures += Check(BCollision2D_ResolveMovement(&document, mover, 1, 0, &resolved) &&
                          resolved.x == 1,
                      "triggers and disabled solids do not block movement");
    BWorkspaceDocument_Destroy(&document);

    MakeDocument(&document);
    size_t noCollider = AddEntity(&document, "No collider", (BTransform2D){0, 0}, false, false, true);
    failures += Check(!BCollision2D_ResolveMovement(&document, noCollider, 1, 0, &resolved),
                      "missing Collider2D fails cleanly");
    mover = AddEntity(&document, "Broken transform", (BTransform2D){0, 0}, true, false, true);
    BWorkspaceComponent *component = BWorkspaceEntity_FindComponent(
        &document.entities[mover], BWORKSPACE_TRANSFORM2D_TYPE);
    component->kind = BWORKSPACE_COMPONENT_UNKNOWN;
    failures += Check(!BCollision2D_ResolveMovement(&document, mover, 1, 0, &resolved),
                      "missing supported Transform2D fails cleanly");
    BWorkspaceDocument_Destroy(&document);

    MakeDocument(&document);
    mover = AddEntity(&document, "Mover", (BTransform2D){0, 0}, true, false, true);
    for (int i = 0; i < 33; ++i)
        AddEntity(&document, "Dense trigger", (BTransform2D){1, 0}, true, true, true);
    AddEntity(&document, "Solid after triggers", (BTransform2D){1, 0}, true, false, true);
    failures += Check(BCollision2D_ResolveMovement(&document, mover, 1, 0, &resolved) &&
                          resolved.x == 0,
                      "solid after more than 32 overlapping triggers still blocks");
    BWorkspaceDocument_Destroy(&document);
    return failures;
}

static int TestTriggers(void)
{
    int failures = 0;
    BWorkspaceDocument document;
    MakeDocument(&document);
    size_t mover = AddEntity(&document, "Mover", (BTransform2D){0, 0}, true, false, true);
    size_t requested = AddEntity(&document, "Requested", (BTransform2D){0, 0}, true, true, true);
    size_t other = AddEntity(&document, "Other", (BTransform2D){0, 0}, true, true, true);
    size_t solid = AddEntity(&document, "Solid", (BTransform2D){0, 0}, true, false, true);
    size_t disabled = AddEntity(&document, "Disabled", (BTransform2D){0, 0}, true, true, false);
    failures += Check(BCollision2D_TriggerOverlapping(&document, mover, requested),
                      "requested overlapping trigger is detected");
    BWorkspaceComponent *transform = BWorkspaceEntity_FindComponent(
        &document.entities[requested], BWORKSPACE_TRANSFORM2D_TYPE);
    transform->data.transform2d.x = 10;
    failures += Check(!BCollision2D_TriggerOverlapping(&document, mover, requested) &&
                          BCollision2D_TriggerOverlapping(&document, mover, other),
                      "only the requested trigger entity is compared");
    failures += Check(!BCollision2D_TriggerOverlapping(&document, mover, solid),
                      "requested non-trigger is rejected");
    failures += Check(!BCollision2D_TriggerOverlapping(&document, mover, disabled),
                      "disabled trigger is rejected");
    BWorkspaceDocument_Destroy(&document);
    return failures;
}

int main(void)
{
    int failures = 0;
    BCollisionAABB bounds = {0};
    BTransform2D transform = {10, -4};
    BCollider2D collider = {2, 3, 4, 6, true};
    failures += Check(BCollision2D_MakeAABB(&transform, &collider, &bounds) &&
                          bounds.minX == 10 && bounds.maxX == 14 && bounds.minY == -4 &&
                          bounds.maxY == 2,
                      "AABB uses Transform2D, Collider2D offset, and dimensions");
    BCollisionAABB left = {0, 0, 2, 2};
    BCollisionAABB overlap = {1, 1, 3, 3};
    BCollisionAABB touching = {2, 0, 4, 2};
    failures += Check(BCollision2D_Overlaps(&left, &overlap) &&
                          !BCollision2D_Overlaps(&left, &touching),
                      "overlap excludes edge touching");

    BWorkspaceDocument document;
    MakeDocument(&document);
    size_t first = AddEntity(&document, "First", (BTransform2D){0, 0}, true, true, true);
    size_t second = AddEntity(&document, "Second", (BTransform2D){.25f, 0}, true, true, true);
    AddEntity(&document, "Disabled", (BTransform2D){0, 0}, true, true, false);
    BCollisionAABB query = {-1, -1, 1, 1};
    BCollisionHit oneHit[1] = {{0}};
    failures += Check(BCollision2D_Query(&document, &query, BCOLLISION_ENTITY_NONE, oneHit, 1) == 2 &&
                          oneHit[0].entityIndex == first,
                      "query count exceeds safe hit-buffer capacity");
    BCollisionHit hits[2] = {{0}};
    failures += Check(BCollision2D_Query(&document, &query, first, hits, 2) == 1 &&
                          hits[0].entityIndex == second,
                      "query ignores requested and disabled entities");
    BWorkspaceDocument_Destroy(&document);

    failures += TestMovement();
    failures += TestTriggers();
    if (failures == 0) printf("BCollision2DTests passed.\n");
    return failures == 0 ? 0 : 1;
}
