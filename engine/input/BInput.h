#ifndef BASIL_ENGINE_INPUT_H
#define BASIL_ENGINE_INPUT_H

#include <stdbool.h>

#define BINPUT_ACTION_NAME_MAX 64
#define BINPUT_MAX_ACTIONS 128

typedef struct BInputAction
{
    char name[BINPUT_ACTION_NAME_MAX];
    int key;
} BInputAction;

void BInput_Init();
void BInput_Shutdown();


bool BInput_SetDefaultActions();
bool BInput_RegisterAction(const char* actionName, int key);
bool BInput_RebindAction(const char* actionName, int newKey);
bool BInput_HasAction(const char* actionName);

bool BInput_IsActionPressed(const char* actionName);
bool BInput_IsActionDown(const char* actionName);
bool BInput_IsActionReleased(const char* actionName);

int BInput_GetActionKey(const char* actionName);
int BInput_GetActionCount();

#endif
