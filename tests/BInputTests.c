#include <stdio.h>

#include "BInput.h"

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

    BInput_Init();

    failures += Check(BInput_GetActionCount() == 7, "default action count");
    failures += Check(BInput_HasAction("move_up"), "move_up is registered");
    failures += Check(!BInput_HasAction("missing"), "unknown action is absent");
    failures += Check(!BInput_RegisterAction("move_up", 123), "duplicates are rejected");
    failures += Check(BInput_RegisterAction("test_action", 123), "new action is accepted");
    failures += Check(BInput_RebindAction("test_action", 456), "action can be rebound");
    failures += Check(BInput_GetActionKey("test_action") == 456, "rebound key is retained");
    failures += Check(!BInput_RebindAction("missing", 456), "unknown action cannot be rebound");

    BInput_Shutdown();
    failures += Check(BInput_GetActionCount() == 0, "shutdown clears actions");

    if (failures == 0)
        printf("BInputTests passed.\n");

    return failures == 0 ? 0 : 1;
}

