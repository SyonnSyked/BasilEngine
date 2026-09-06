#ifndef WHERE_BIRDS_NEST_GAMEPLAY_H
#define WHERE_BIRDS_NEST_GAMEPLAY_H

#include "BGame.h"

bool WBN_MoveWithCollision(const BGameHostAPI *host, BGameEntity entity, float deltaX,
                           float deltaY);
bool WBN_IsTriggerOverlapping(const BGameHostAPI *host, BGameEntity entity,
                              BGameEntity triggerEntity);

#endif
