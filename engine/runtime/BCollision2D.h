#ifndef BASIL_ENGINE_COLLISION_2D_H
#define BASIL_ENGINE_COLLISION_2D_H

#include "BWorkspace.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BCOLLISION_ENTITY_NONE ((size_t)-1)

typedef struct BCollisionAABB {
    float minX;
    float minY;
    float maxX;
    float maxY;
} BCollisionAABB;

typedef struct BCollisionHit {
    size_t entityIndex;
    BCollisionAABB bounds;
    bool trigger;
} BCollisionHit;

bool BCollision2D_MakeAABB(const BTransform2D *transform, const BCollider2D *collider,
                           BCollisionAABB *bounds);

bool BCollision2D_EntityAABB(const BWorkspaceEntity *entity, BCollisionAABB *bounds, bool *trigger);

bool BCollision2D_Overlaps(const BCollisionAABB *left, const BCollisionAABB *right);

size_t BCollision2D_Query(const BWorkspaceDocument *document, const BCollisionAABB *area,
                          size_t ignoreEntityIndex, BCollisionHit *hits, size_t hitCapacity);

bool BCollision2D_ResolveMovement(const BWorkspaceDocument *document, size_t entityIndex,
                                  float deltaX, float deltaY,
                                  BTransform2D *resolvedTransform);

bool BCollision2D_TriggerOverlapping(const BWorkspaceDocument *document, size_t entityIndex,
                                     size_t triggerEntityIndex);

#ifdef __cplusplus
}
#endif

#endif
