#include <stdio.h>

#include "WBNCombat.h"

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

int main(void)
{
    int failures = 0;
    WBNCombatTarget target;
    WBNCombatTarget_Init(&target, (WBNPosition){ 3.0f, 4.0f }, 3);

    failures += Check(target.health == 3 && target.maxHealth == 3, "target initializes");
    failures += Check(WBNCombatTarget_IsAlive(&target), "new target is alive");
    failures += Check(
        WBNCombat_TryBasicAttack((WBNPosition){ 0.0f, 0.0f }, &target, 4.9f, 1) ==
            WBN_ATTACK_OUT_OF_RANGE,
        "attack outside radius misses"
    );
    failures += Check(target.health == 3, "miss does not change health");
    failures += Check(
        WBNCombat_TryBasicAttack((WBNPosition){ 0.0f, 0.0f }, &target, 5.0f, 1) ==
            WBN_ATTACK_HIT,
        "attack on range boundary hits"
    );
    failures += Check(target.health == 2, "hit applies damage");
    failures += Check(
        WBNCombat_TryBasicAttack((WBNPosition){ 3.0f, 4.0f }, &target, 1.0f, 2) ==
            WBN_ATTACK_KILLED,
        "lethal attack reports kill"
    );
    failures += Check(!WBNCombatTarget_IsAlive(&target), "killed target is not alive");
    failures += Check(target.health == 0, "health is clamped to zero");
    failures += Check(
        WBNCombat_TryBasicAttack((WBNPosition){ 3.0f, 4.0f }, &target, 1.0f, 1) ==
            WBN_ATTACK_INVALID,
        "dead target cannot be attacked"
    );

    if (failures == 0)
        printf("WBNCombatTests passed.\n");

    return failures == 0 ? 0 : 1;
}

