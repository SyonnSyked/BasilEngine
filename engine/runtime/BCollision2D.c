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

static bool BCollision2D_AxisBlocked(const BWorkspaceDocument *document, size_t entityIndex,
                                     const BCollisionAABB *candidate)
{
    for (size_t otherIndex = 0; otherIndex < document->entityCount; ++otherIndex) {
        if (otherIndex == entityIndex || !document->entities[otherIndex].enabled)
            continue;

        BCollisionAABB otherBounds;
        bool trigger = false;
        if (BCollision2D_EntityAABB(&document->entities[otherIndex], &otherBounds, &trigger) &&
            !trigger && BCollision2D_Overlaps(candidate, &otherBounds)) {
            return true;
        }
    }
    return false;
}

bool BCollision2D_ResolveMovement(const BWorkspaceDocument *document, size_t entityIndex,
                                  float deltaX, float deltaY, BTransform2D *resolvedTransform)
{
    if (document == NULL || resolvedTransform == NULL || entityIndex >= document->entityCount ||
        !isfinite(deltaX) || !isfinite(deltaY)) {
        return false;
    }

    const BWorkspaceEntity *entity = &document->entities[entityIndex];
    if (!entity->enabled)
        return false;

    const BWorkspaceComponent *transformComponent =
        BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_TRANSFORM2D_TYPE);
    const BWorkspaceComponent *colliderComponent =
        BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_COLLIDER2D_TYPE);
    if (transformComponent == NULL || colliderComponent == NULL ||
        transformComponent->kind != BWORKSPACE_COMPONENT_TRANSFORM2D ||
        colliderComponent->kind != BWORKSPACE_COMPONENT_COLLIDER2D) {
        return false;
    }

    BTransform2D accepted = transformComponent->data.transform2d;
    BCollisionAABB bounds;
    if (!BCollision2D_MakeAABB(&accepted, &colliderComponent->data.collider2d, &bounds))
        return false;

    if (deltaX != 0.0f) {
        BTransform2D candidateTransform = accepted;
        candidateTransform.x += deltaX;
        BCollisionAABB candidate;
        if (!BCollision2D_MakeAABB(&candidateTransform, &colliderComponent->data.collider2d,
                                   &candidate)) {
            return false;
        }
        if (!BCollision2D_AxisBlocked(document, entityIndex, &candidate)) {
            accepted = candidateTransform;
        }
    }
    if (deltaY != 0.0f) {
        BTransform2D candidateTransform = accepted;
        candidateTransform.y += deltaY;
        BCollisionAABB candidate;
        if (!BCollision2D_MakeAABB(&candidateTransform, &colliderComponent->data.collider2d,
                                   &candidate)) {
            return false;
        }
        if (!BCollision2D_AxisBlocked(document, entityIndex, &candidate)) {
            accepted = candidateTransform;
        }
    }

    *resolvedTransform = accepted;
    return true;
}

bool BCollision2D_TriggerOverlapping(const BWorkspaceDocument *document, size_t entityIndex,
                                     size_t triggerEntityIndex)
{
    if (document == NULL || entityIndex >= document->entityCount ||
        triggerEntityIndex >= document->entityCount) {
        return false;
    }

    const BWorkspaceEntity *entity = &document->entities[entityIndex];
    const BWorkspaceEntity *triggerEntity = &document->entities[triggerEntityIndex];
    if (!entity->enabled || !triggerEntity->enabled)
        return false;

    BCollisionAABB entityBounds;
    BCollisionAABB triggerBounds;
    bool targetIsTrigger = false;
    return BCollision2D_EntityAABB(entity, &entityBounds, NULL) &&
           BCollision2D_EntityAABB(triggerEntity, &triggerBounds, &targetIsTrigger) &&
           targetIsTrigger && BCollision2D_Overlaps(&entityBounds, &triggerBounds);
}
