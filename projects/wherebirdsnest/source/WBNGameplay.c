#include "WBNGameplay.h"

static bool WBN_AxisBlocked(const BGameHostAPI *host, BGameEntity entity, BGameAABB bounds)
{
    BGameCollisionHit hits[32];
    size_t count = host->queryColliders(host->context, &bounds, entity, hits, 32);
    size_t stored = count < 32 ? count : 32;
    for (size_t i = 0; i < stored; ++i)
        if (!hits[i].trigger)
            return true;
    return false;
}

bool WBN_MoveWithCollision(const BGameHostAPI *host, BGameEntity entity, float deltaX,
                           float deltaY)
{
    if (host == NULL || host->getPosition == NULL || host->setPosition == NULL ||
        host->getColliderBounds == NULL || host->queryColliders == NULL)
        return false;
    float x = 0.0f;
    float y = 0.0f;
    BGameAABB bounds;
    if (!host->getPosition(host->context, entity, &x, &y) ||
        !host->getColliderBounds(host->context, entity, &bounds, NULL))
        return false;
    bool moved = false;
    if (deltaX != 0.0f) {
        BGameAABB candidate = bounds;
        candidate.minX += deltaX;
        candidate.maxX += deltaX;
        if (!WBN_AxisBlocked(host, entity, candidate)) {
            x += deltaX;
            bounds = candidate;
            moved = true;
        }
    }
    if (deltaY != 0.0f) {
        BGameAABB candidate = bounds;
        candidate.minY += deltaY;
        candidate.maxY += deltaY;
        if (!WBN_AxisBlocked(host, entity, candidate)) {
            y += deltaY;
            moved = true;
        }
    }
    return moved && host->setPosition(host->context, entity, x, y);
}

bool WBN_IsTriggerOverlapping(const BGameHostAPI *host, BGameEntity entity,
                              BGameEntity triggerEntity)
{
    if (host == NULL || host->getColliderBounds == NULL || host->queryColliders == NULL ||
        triggerEntity.value == 0)
        return false;
    BGameAABB bounds;
    if (!host->getColliderBounds(host->context, entity, &bounds, NULL))
        return false;
    BGameCollisionHit hits[32];
    size_t count = host->queryColliders(host->context, &bounds, entity, hits, 32);
    size_t stored = count < 32 ? count : 32;
    for (size_t i = 0; i < stored; ++i)
        if (hits[i].entity.value == triggerEntity.value && hits[i].trigger)
            return true;
    return false;
}
