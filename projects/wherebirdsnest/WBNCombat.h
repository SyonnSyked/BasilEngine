#ifndef WHERE_BIRDS_NEST_COMBAT_H
#define WHERE_BIRDS_NEST_COMBAT_H

#include <stdbool.h>

typedef struct WBNPosition
{
    float x;
    float y;
} WBNPosition;

typedef struct WBNCombatTarget
{
    WBNPosition position;
    int health;
    int maxHealth;
} WBNCombatTarget;

typedef enum WBNBasicAttackResult
{
    WBN_ATTACK_INVALID,
    WBN_ATTACK_OUT_OF_RANGE,
    WBN_ATTACK_HIT,
    WBN_ATTACK_KILLED
} WBNBasicAttackResult;

void WBNCombatTarget_Init(
    WBNCombatTarget* target,
    WBNPosition position,
    int maxHealth
);

bool WBNCombatTarget_IsAlive(const WBNCombatTarget* target);

WBNBasicAttackResult WBNCombat_TryBasicAttack(
    WBNPosition attackerPosition,
    WBNCombatTarget* target,
    float range,
    int damage
);

#endif

