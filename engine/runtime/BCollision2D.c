#include "BCollision2D.h"

#include <math.h>

static bool BCollision2D_IsValidAABB(const BCollisionAABB *bounds)
{
    return bounds != NULL && isfinite(bounds->minX) && isfinite(bounds->minY) &&
           isfinite(bounds->maxX) && isfinite(bounds->maxY) && bounds->minX < bounds->maxX &&
           bounds->minY < bounds->maxY;
}

bool BCollision2D_MakeAABB(const BTransform2D *transform, const BCollider2D *collider,
                           BCollisionAABB *bounds)
{
    if (transform == NULL || collider == NULL || bounds == NULL) {
        return false;
    }

    if (!isfinite(transform->x) || !isfinite(transform->y) || !isfinite(collider->offsetX) ||
        !isfinite(collider->offsetY) || !isfinite(collider->width) || !isfinite(collider->height) ||
        collider->width <= 0.0f || collider->height <= 0.0f) {
        return false;
    }

    float centerX = transform->x + collider->offsetX;

    float centerY = transform->y + collider->offsetY;

    float halfWidth = collider->width * 0.5f;

    float halfHeight = collider->height * 0.5f;

    BCollisionAABB result = {.minX = centerX - halfWidth,
                             .minY = centerY - halfHeight,
                             .maxX = centerX + halfWidth,
                             .maxY = centerY + halfHeight};

    if (!BCollision2D_IsValidAABB(&result)) {
        return false;
    }

    *bounds = result;

    return true;
}

bool BCollision2D_EntityAABB(const BWorkspaceEntity *entity, BCollisionAABB *bounds, bool *trigger)
{
    if (entity == NULL || bounds == NULL) {
        return false;
    }

    const BWorkspaceComponent *transformComponent =
        BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_TRANSFORM2D_TYPE);

    const BWorkspaceComponent *colliderComponent =
        BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_COLLIDER2D_TYPE);

    if (transformComponent == NULL || colliderComponent == NULL ||
        transformComponent->kind != BWORKSPACE_COMPONENT_TRANSFORM2D ||
        colliderComponent->kind != BWORKSPACE_COMPONENT_COLLIDER2D) {
        return false;
    }

    if (!BCollision2D_MakeAABB(&transformComponent->data.transform2d,
                               &colliderComponent->data.collider2d, bounds)) {
        return false;
    }

    if (trigger != NULL) {
        *trigger = colliderComponent->data.collider2d.trigger;
    }

    return true;
}

bool BCollision2D_Overlaps(const BCollisionAABB *left, const BCollisionAABB *right)
{
    if (!BCollision2D_IsValidAABB(left) || !BCollision2D_IsValidAABB(right)) {
        return false;
    }

    return left->minX < right->maxX && left->maxX > right->minX && left->minY < right->maxY &&
           left->maxY > right->minY;
}

size_t BCollision2D_Query(const BWorkspaceDocument *document, const BCollisionAABB *area,
                          size_t ignoreEntityIndex, BCollisionHit *hits, size_t hitCapacity)
{
    if (document == NULL || !BCollision2D_IsValidAABB(area)) {
        return 0;
    }

    size_t hitCount = 0;

    for (size_t entityIndex = 0; entityIndex < document->entityCount; ++entityIndex) {
        if (entityIndex == ignoreEntityIndex) {
            continue;
        }

        const BWorkspaceEntity *entity = &document->entities[entityIndex];

        if (!entity->enabled) {
            continue;
        }

        BCollisionAABB bounds;
        bool trigger = false;

        if (!BCollision2D_EntityAABB(entity, &bounds, &trigger)) {
            continue;
        }

        if (!BCollision2D_Overlaps(area, &bounds)) {
            continue;
        }

        if (hits != NULL && hitCount < hitCapacity) {
            hits[hitCount].entityIndex = entityIndex;

            hits[hitCount].bounds = bounds;

            hits[hitCount].trigger = trigger;
        }

        ++hitCount;
    }

    return hitCount;
}
