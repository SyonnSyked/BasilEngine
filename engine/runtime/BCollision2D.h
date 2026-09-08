#ifndef BASIL_ENGINE_COLLISION_2D_H
#define BASIL_ENGINE_COLLISION_2D_H

#include "BGame.h"
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

bool MoveWithCollision(const BGameHostAPI *host, BGameEntity entity, float deltaX, float deltaY);
bool IsTriggerOverlapping(const BGameHostAPI *host, BGameEntity entity, BGameEntity triggerEntity);

#ifdef __cplusplus
}
#endif

#endif
