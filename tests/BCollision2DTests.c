#include "BCollision2D.h"

#include <math.h>
#include <stdio.h>

static int Check(bool condition, const char *message)
{
    if (condition) return 0;
    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

static size_t AddColliderEntity(BWorkspaceDocument *document, const char *name,
                                BTransform2D transform, BCollider2D collider, bool enabled)
{
    BDiagnosticList diagnostics = {0};
    size_t index = 0;
    if (!BWorkspaceDocument_AddEntity(document, name, &index, &diagnostics) ||
        !BWorkspaceDocument_AddTransform2D(document, index, transform, false, &diagnostics) ||
        !BWorkspaceDocument_AddCollider2D(document, index, collider, false, &diagnostics)) {
        return BCOLLISION_ENTITY_NONE;
    }
    document->entities[index].enabled = enabled;
    return index;
}

int main(void)
{
    int failures = 0;
    BCollisionAABB bounds = {0};
    BTransform2D transform = {10.0f, -4.0f};
    BCollider2D collider = {2.0f, 3.0f, 4.0f, 6.0f, true};
    failures += Check(BCollision2D_MakeAABB(&transform, &collider, &bounds) &&
                          bounds.minX == 10.0f && bounds.maxX == 14.0f &&
                          bounds.minY == -4.0f && bounds.maxY == 2.0f,
                      "AABB uses position, offset, and dimensions");
    BCollider2D invalid = collider;
    invalid.width = 0.0f;
    failures += Check(!BCollision2D_MakeAABB(&transform, &invalid, &bounds),
                      "zero width is rejected");
    invalid.width = NAN;
    failures += Check(!BCollision2D_MakeAABB(&transform, &invalid, &bounds),
                      "non-finite dimensions are rejected");

    BCollisionAABB left = {0.0f, 0.0f, 2.0f, 2.0f};
    BCollisionAABB overlap = {1.0f, 1.0f, 3.0f, 3.0f};
    BCollisionAABB touching = {2.0f, 0.0f, 4.0f, 2.0f};
    failures += Check(BCollision2D_Overlaps(&left, &overlap), "overlap is detected");
    failures += Check(!BCollision2D_Overlaps(&left, &touching), "edge touch is not overlap");

    BWorkspaceDocument document;
    BWorkspaceDocument_Init(&document);
    BDiagnosticList diagnostics = {0};
    failures += Check(BWorkspaceDocument_CreateDefault(&document, "Collision", "Collision",
                                                       &diagnostics), "document is created");
    BCollider2D unit = BCollider2D_Default();
    unit.trigger = true;
    size_t first = AddColliderEntity(&document, "First", (BTransform2D){0.0f, 0.0f}, unit, true);
    size_t second = AddColliderEntity(&document, "Second", (BTransform2D){0.25f, 0.0f}, unit, true);
    (void)AddColliderEntity(&document, "Disabled", (BTransform2D){0.0f, 0.0f}, unit, false);
    size_t noCollider = 0;
    failures += Check(BWorkspaceDocument_AddEntity(&document, "No Collider", &noCollider,
                                                   &diagnostics) &&
                          BWorkspaceDocument_AddTransform2D(&document, noCollider,
                                                            (BTransform2D){0.0f, 0.0f}, false,
                                                            &diagnostics),
                      "non-collider entity is created");
    bool trigger = false;
    failures += Check(BCollision2D_EntityAABB(&document.entities[first], &bounds, &trigger) && trigger,
                      "entity AABB requires components and propagates trigger");
    failures += Check(!BCollision2D_EntityAABB(&document.entities[noCollider], &bounds, &trigger),
                      "entity without collider has no AABB");
    BCollisionAABB query = {-1.0f, -1.0f, 1.0f, 1.0f};
    BCollisionHit oneHit[1] = {{0}};
    failures += Check(BCollision2D_Query(&document, &query, BCOLLISION_ENTITY_NONE, oneHit, 1) == 2 &&
                          oneHit[0].entityIndex == first,
                      "query count exceeds safe hit-buffer capacity");
    BCollisionHit hits[4] = {{0}};
    failures += Check(BCollision2D_Query(&document, &query, first, hits, 4) == 1 &&
                          hits[0].entityIndex == second,
                      "query ignores requested and disabled entities");
    BWorkspaceDocument_Destroy(&document);
    if (failures == 0) printf("BCollision2DTests passed.\n");
    return failures == 0 ? 0 : 1;
}
