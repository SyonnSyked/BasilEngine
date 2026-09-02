#ifndef BASIL_ENGINE_INPUT_H
#define BASIL_ENGINE_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#define BINPUT_ACTION_NAME_MAX 64
#define BINPUT_MAX_ACTIONS 128

typedef struct BInputAction
{
    char name[BINPUT_ACTION_NAME_MAX];
    int code;
    int device;
} BInputAction;

typedef enum BInputDevice { BINPUT_DEVICE_KEYBOARD, BINPUT_DEVICE_MOUSE } BInputDevice;

void BInput_Init();
void BInput_Shutdown();


bool BInput_SetDefaultActions();
bool BInput_RegisterAction(const char* actionName, int key);
bool BInput_RegisterMouseAction(const char* actionName, int button);
bool BInput_RebindAction(const char* actionName, int newKey);
bool BInput_RebindMouseAction(const char* actionName, int newButton);
bool BInput_HasAction(const char* actionName);

bool BInput_IsActionPressed(const char* actionName);
bool BInput_IsActionDown(const char* actionName);
bool BInput_IsActionReleased(const char* actionName);

int BInput_GetActionKey(const char* actionName);
int BInput_GetActionCode(const char* actionName);
BInputDevice BInput_GetActionDevice(const char* actionName);
int BInput_GetActionCount();
void BInput_SetFocusSuppressed(bool suppressed);
bool BInput_IsFocusSuppressed();
bool BInput_LoadActionMap(const char* path, char* error, size_t errorSize);

#endif
