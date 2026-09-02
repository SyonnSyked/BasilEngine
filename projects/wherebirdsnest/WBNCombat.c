#include "WBNCombat.h"

void WBNCombatTarget_Init(
    WBNCombatTarget* target,
    WBNPosition position,
    int maxHealth
)
{
    if (target == 0)
        return;

    target->position = position;
    target->maxHealth = maxHealth > 0 ? maxHealth : 0;
    target->health = target->maxHealth;
}

bool WBNCombatTarget_IsAlive(const WBNCombatTarget* target)
{
    return target != 0 && target->health > 0;
}

WBNBasicAttackResult WBNCombat_TryBasicAttack(
    WBNPosition attackerPosition,
    WBNCombatTarget* target,
    float range,
    int damage
)
{
    if (target == 0 || !WBNCombatTarget_IsAlive(target) || range < 0.0f || damage <= 0)
        return WBN_ATTACK_INVALID;

    float deltaX = target->position.x - attackerPosition.x;
    float deltaY = target->position.y - attackerPosition.y;
    float distanceSquared = deltaX * deltaX + deltaY * deltaY;

    if (distanceSquared > range * range)
        return WBN_ATTACK_OUT_OF_RANGE;

    target->health -= damage;

    if (target->health <= 0)
    {
        target->health = 0;
        return WBN_ATTACK_KILLED;
    }

    return WBN_ATTACK_HIT;
}

