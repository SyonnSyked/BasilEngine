#include <stdio.h>

#include "BInput.h"

static int Check(bool condition, const char *message)
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

    failures += Check(BInput_GetActionCount() == 6, "default action count");
    failures += Check(BInput_HasAction("move_up"), "move_up is registered");
    failures += Check(!BInput_HasAction("missing"), "unknown action is absent");
    failures += Check(!BInput_RegisterAction("move_up", 123), "duplicates are rejected");
    failures += Check(BInput_RegisterAction("test_action", 123), "new action is accepted");
    failures += Check(BInput_RebindAction("test_action", 456), "action can be rebound");
    failures += Check(BInput_GetActionKey("test_action") == 456, "rebound key is retained");
    failures += Check(!BInput_RebindAction("missing", 456), "unknown action cannot be rebound");
    failures += Check(BInput_RegisterMouseAction("primary", 0), "mouse action registers");

    failures += Check(BInput_GetActionDevice("primary") == BINPUT_DEVICE_MOUSE &&
                          BInput_GetActionCode("primary") == 0,
                      "mouse binding is retained");

    failures += Check(BInput_RebindMouseAction("primary", 2), "mouse action can be rebound");

    failures += Check(BInput_GetActionDevice("primary") == BINPUT_DEVICE_MOUSE &&
                          BInput_GetActionCode("primary") == 2,
                      "mouse rebind is retained");

    failures +=
        Check(BInput_RebindAction("primary", 321), "mouse action can be rebound to keyboard");

    failures += Check(BInput_GetActionDevice("primary") == BINPUT_DEVICE_KEYBOARD &&
                          BInput_GetActionCode("primary") == 321,
                      "keyboard rebind changes the binding device");

    failures +=
        Check(BInput_RebindMouseAction("primary", 1), "keyboard action can be rebound to mouse");

    failures += Check(BInput_GetActionDevice("primary") == BINPUT_DEVICE_MOUSE &&
                          BInput_GetActionCode("primary") == 1,
                      "mouse rebind changes the binding device");

    failures +=
        Check(!BInput_RebindMouseAction("missing", 1), "unknown action cannot be rebound to mouse");

    failures +=
        Check(!BInput_RebindMouseAction("primary", -1), "negative mouse binding is rejected");
    BInput_SetFocusSuppressed(true);
    failures += Check(BInput_IsFocusSuppressed() && !BInput_IsActionDown("move_up"),
                      "focus suppression blocks action state");
    BInput_SetFocusSuppressed(false);

    const char *mapPath = "BInputMapTest.json";
    FILE *map = fopen(mapPath, "wb");
    fputs("{\"schemaVersion\":1,\"actions\":[{\"name\":\"attack\",\"device\":\"mouse\",\"code\":1},"
          "{\"name\":\"dash\",\"device\":\"keyboard\",\"code\":32}]}",
          map);
    fclose(map);
    char error[256];
    failures +=
        Check(BInput_LoadActionMap(mapPath, error, sizeof(error)), "Project action map loads");
    failures += Check(BInput_GetActionCount() == 2 &&
                          BInput_GetActionDevice("attack") == BINPUT_DEVICE_MOUSE,
                      "Project map replaces bindings transactionally");
    map = fopen(mapPath, "wb");
    fputs("{bad", map);
    fclose(map);
    failures +=
        Check(!BInput_LoadActionMap(mapPath, error, sizeof(error)) && BInput_HasAction("dash"),
              "malformed map preserves last valid bindings");
    remove(mapPath);

    BInput_Shutdown();
    failures += Check(BInput_GetActionCount() == 0, "shutdown clears actions");

    if (failures == 0)
        printf("BInputTests passed.\n");

    return failures == 0 ? 0 : 1;
}
