#include "WBNGameplay.h"

#include <stdio.h>

typedef struct TestWorld {
    float playerX;
    float playerY;
    bool includeSolid;
    bool includeTrigger;
} TestWorld;

static bool Overlaps(BGameAABB a, BGameAABB b)
{
    return a.minX < b.maxX && a.maxX > b.minX && a.minY < b.maxY && a.maxY > b.minY;
}

static bool GetPosition(void *context, BGameEntity entity, float *x, float *y)
{
    TestWorld *world = context;
    if (entity.value != 1)
        return false;
    *x = world->playerX;
    *y = world->playerY;
    return true;
}

static bool SetPosition(void *context, BGameEntity entity, float x, float y)
{
    TestWorld *world = context;
    if (entity.value != 1)
        return false;
    world->playerX = x;
    world->playerY = y;
    return true;
}

static bool GetBounds(void *context, BGameEntity entity, BGameAABB *bounds, bool *trigger)
{
    TestWorld *world = context;
    if (entity.value == 1)
        *bounds = (BGameAABB){world->playerX - 0.5f, world->playerY - 0.5f,
                              world->playerX + 0.5f, world->playerY + 0.5f};
    else if (entity.value == 2)
        *bounds = (BGameAABB){1.0f, -1.0f, 2.0f, 1.0f};
    else if (entity.value == 3)
        *bounds = (BGameAABB){-1.0f, -1.0f, 1.0f, 1.0f};
    else
        return false;
    if (trigger != NULL)
        *trigger = entity.value == 3;
    return true;
}

static size_t Query(void *context, const BGameAABB *area, BGameEntity ignored,
                    BGameCollisionHit *hits, size_t capacity)
{
    TestWorld *world = context;
    size_t count = 0;
    BGameAABB bounds;
    if (world->includeSolid && ignored.value != 2 &&
        GetBounds(context, (BGameEntity){2}, &bounds, NULL) && Overlaps(*area, bounds)) {
        if (hits != NULL && count < capacity)
            hits[count] = (BGameCollisionHit){(BGameEntity){2}, bounds, false};
        ++count;
    }
    if (world->includeTrigger && ignored.value != 3 &&
        GetBounds(context, (BGameEntity){3}, &bounds, NULL) && Overlaps(*area, bounds)) {
        if (hits != NULL && count < capacity)
            hits[count] = (BGameCollisionHit){(BGameEntity){3}, bounds, true};
        ++count;
    }
    return count;
}

static int Check(bool condition, const char *message)
{
    if (condition)
        return 0;
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    int failures = 0;
    TestWorld world = {0};
    BGameHostAPI host = {.context = &world,
                         .getPosition = GetPosition,
                         .setPosition = SetPosition,
                         .getColliderBounds = GetBounds,
                         .queryColliders = Query};
    world.includeSolid = true;
    failures += Check(!WBN_MoveWithCollision(&host, (BGameEntity){1}, 1.0f, 0.0f) &&
                          world.playerX == 0.0f,
                      "solid overlap blocks movement");
    world.includeSolid = false;
    world.includeTrigger = true;
    failures += Check(WBN_MoveWithCollision(&host, (BGameEntity){1}, 0.25f, 0.0f) &&
                          world.playerX == 0.25f,
                      "trigger overlap does not block movement");
    failures += Check(WBN_IsTriggerOverlapping(&host, (BGameEntity){1}, (BGameEntity){3}),
                      "interaction trigger is detected");
    world.includeTrigger = false;
    failures += Check(!WBN_IsTriggerOverlapping(&host, (BGameEntity){1}, (BGameEntity){3}),
                      "absent interaction trigger is not detected");
    if (failures == 0)
        printf("WBNGameplayTests passed.\n");
    return failures == 0 ? 0 : 1;
}
