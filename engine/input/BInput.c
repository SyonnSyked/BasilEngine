#include "BInput.h"

#include "BLog.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

static BInputAction g_Actions[BINPUT_MAX_ACTIONS];
static int g_ActionCount = 0;

static int BInput_FindActionIndex(const char* actionName)
{
    if (actionName == NULL)
    {
        return -1;
    }

    for (int i = 0; i < g_ActionCount; i++)
    {
        if (strcmp(g_Actions[i].name, actionName) == 0)
        {
            return i;
        }
    }

    return -1;
}

void BInput_Init()
{
    g_ActionCount = 0;

    BLog_Info("Input system initialized.");

    if (!BInput_SetDefaultActions())
        BLog_Error("Input defaults failed setup- See function: BInput_Init()");
}

bool BInput_SetDefaultActions() {

    bool succeeded = true;

    succeeded = BInput_RegisterAction("toggle_console", KEY_GRAVE) && succeeded;
    succeeded = BInput_RegisterAction("move_up", KEY_W) && succeeded;
    succeeded = BInput_RegisterAction("move_down", KEY_S) && succeeded;
    succeeded = BInput_RegisterAction("move_left", KEY_A) && succeeded;
    succeeded = BInput_RegisterAction("move_right", KEY_D) && succeeded;
    succeeded = BInput_RegisterAction("confirm", KEY_ENTER) && succeeded;
    succeeded = BInput_RegisterAction("cancel", KEY_ESCAPE) && succeeded;

    BLog_Info("Input Defaults initialized...");

    return succeeded;
}

void BInput_Shutdown()
{
    g_ActionCount = 0;

    BLog_Info("Input system shutdown.");
}

bool BInput_RegisterAction(const char* actionName, int key)
{
    if (actionName == NULL || actionName[0] == '\0')
    {
        BLog_Warning("BInput_RegisterAction failed: action name was empty.");
        return false;
    }

    if (g_ActionCount >= BINPUT_MAX_ACTIONS)
    {
        BLog_Warning("BInput_RegisterAction failed: max action count reached.");
        return false;
    }

    if (BInput_FindActionIndex(actionName) != -1)
    {
        BLog_WarningF("BInput_RegisterAction failed: action already exists: %s", actionName);
        return false;
    }

    BInputAction* action = &g_Actions[g_ActionCount];

    snprintf(action->name, BINPUT_ACTION_NAME_MAX, "%s", actionName);
    action->key = key;

    g_ActionCount++;

    BLog_DebugF("Registered input action: %s", actionName);

    return true;
}

bool BInput_RebindAction(const char* actionName, int newKey)
{
    int index = BInput_FindActionIndex(actionName);

    if (index == -1)
    {
        BLog_WarningF("BInput_RebindAction failed: unknown action: %s", actionName);
        return false;
    }

    g_Actions[index].key = newKey;

    BLog_InfoF("Rebound input action: %s", actionName);

    return true;
}

bool BInput_HasAction(const char* actionName)
{
    return BInput_FindActionIndex(actionName) != -1;
}

bool BInput_IsActionPressed(const char* actionName)
{
    int index = BInput_FindActionIndex(actionName);

    if (index == -1)
    {
        return false;
    }

    return IsKeyPressed(g_Actions[index].key);
}

bool BInput_IsActionDown(const char* actionName)
{
    int index = BInput_FindActionIndex(actionName);

    if (index == -1)
    {
        return false;
    }

    return IsKeyDown(g_Actions[index].key);
}

bool BInput_IsActionReleased(const char* actionName)
{
    int index = BInput_FindActionIndex(actionName);

    if (index == -1)
    {
        return false;
    }

    return IsKeyReleased(g_Actions[index].key);
}

int BInput_GetActionKey(const char* actionName)
{
    int index = BInput_FindActionIndex(actionName);

    if (index == -1)
    {
        return 0;
    }

    return g_Actions[index].key;
}

int BInput_GetActionCount()
{
    return g_ActionCount;
}
