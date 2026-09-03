#include "BInput.h"
#include "BLog.h"
#include "cJSON.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BInputAction g_Actions[BINPUT_MAX_ACTIONS];
static int g_ActionCount;
static bool g_FocusSuppressed;
static int Find(const char *name)
{
    if (!name)
        return -1;
    for (int i = 0; i < g_ActionCount; ++i)
        if (!strcmp(g_Actions[i].name, name))
            return i;
    return -1;
}
static bool Register(const char *name, BInputDevice device, int code)
{
    if (!name || !name[0] || strlen(name) >= BINPUT_ACTION_NAME_MAX || code < 0 ||
        g_ActionCount >= BINPUT_MAX_ACTIONS || Find(name) >= 0)
        return false;
    BInputAction *action = &g_Actions[g_ActionCount++];
    snprintf(action->name, sizeof(action->name), "%s", name);
    action->device = device;
    action->code = code;
    return true;
}
void BInput_Init()
{
    g_ActionCount = 0;
    g_FocusSuppressed = false;
    BLog_Info("Input system initialized.");
    if (!BInput_SetDefaultActions())
        BLog_Error("Input defaults failed setup.");
}
void BInput_Shutdown()
{
    g_ActionCount = 0;
    g_FocusSuppressed = false;
    BLog_Info("Input system shutdown.");
}
bool BInput_SetDefaultActions()
{
    bool ok = true;
    ok = BInput_RegisterAction("move_up", KEY_W) && ok;
    ok = BInput_RegisterAction("move_down", KEY_S) && ok;
    ok = BInput_RegisterAction("move_left", KEY_A) && ok;
    ok = BInput_RegisterAction("move_right", KEY_D) && ok;
    ok = BInput_RegisterAction("confirm", KEY_ENTER) && ok;
    ok = BInput_RegisterAction("cancel", KEY_ESCAPE) && ok;
    return ok;
}
bool BInput_RegisterAction(const char *name, int key)
{
    return Register(name, BINPUT_DEVICE_KEYBOARD, key);
}
bool BInput_RegisterMouseAction(const char *name, int button)
{
    return Register(name, BINPUT_DEVICE_MOUSE, button);
}
bool BInput_RebindAction(const char *name, int key)
{
    int i = Find(name);
    if (i < 0 || key < 0)
        return false;
    g_Actions[i].device = BINPUT_DEVICE_KEYBOARD;
    g_Actions[i].code = key;
    return true;
}
bool BInput_RebindMouseAction(const char *name, int button)
{
    int i = Find(name);
    if (i < 0 || button < 0)
        return false;
    g_Actions[i].device = BINPUT_DEVICE_MOUSE;
    g_Actions[i].code = button;
    return true;
}
bool BInput_HasAction(const char *name)
{
    return Find(name) >= 0;
}
bool BInput_IsActionPressed(const char *name)
{
    int i = Find(name);
    if (i < 0 || g_FocusSuppressed)
        return false;
    return g_Actions[i].device == BINPUT_DEVICE_MOUSE ? IsMouseButtonPressed(g_Actions[i].code)
                                                      : IsKeyPressed(g_Actions[i].code);
}
bool BInput_IsActionDown(const char *name)
{
    int i = Find(name);
    if (i < 0 || g_FocusSuppressed)
        return false;
    return g_Actions[i].device == BINPUT_DEVICE_MOUSE ? IsMouseButtonDown(g_Actions[i].code)
                                                      : IsKeyDown(g_Actions[i].code);
}
bool BInput_IsActionReleased(const char *name)
{
    int i = Find(name);
    if (i < 0 || g_FocusSuppressed)
        return false;
    return g_Actions[i].device == BINPUT_DEVICE_MOUSE ? IsMouseButtonReleased(g_Actions[i].code)
                                                      : IsKeyReleased(g_Actions[i].code);
}
int BInput_GetActionKey(const char *name)
{
    int i = Find(name);
    return i < 0 || g_Actions[i].device != BINPUT_DEVICE_KEYBOARD ? 0 : g_Actions[i].code;
}
int BInput_GetActionCode(const char *name)
{
    int i = Find(name);
    return i < 0 ? 0 : g_Actions[i].code;
}
BInputDevice BInput_GetActionDevice(const char *name)
{
    int i = Find(name);
    return i < 0 ? BINPUT_DEVICE_KEYBOARD : (BInputDevice)g_Actions[i].device;
}
int BInput_GetActionCount()
{
    return g_ActionCount;
}
void BInput_SetFocusSuppressed(bool value)
{
    g_FocusSuppressed = value;
}
bool BInput_IsFocusSuppressed()
{
    return g_FocusSuppressed;
}

static bool Fail(char *error, size_t size, const char *message)
{
    if (error && size)
        snprintf(error, size, "%s", message);
    return false;
}
bool BInput_LoadActionMap(const char *path, char *error, size_t errorSize)
{
    if (error && errorSize)
        error[0] = '\0';
    FILE *file = path ? fopen(path, "rb") : NULL;
    if (!file)
        return Fail(error, errorSize, "Could not open input action map.");
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    if (length < 0 || length > 64 * 1024) {
        fclose(file);
        return Fail(error, errorSize, "Input action map exceeds 64 KiB.");
    }
    char *text = (char *)malloc((size_t)length + 1);
    bool read = text && fread(text, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (!read) {
        free(text);
        return Fail(error, errorSize, "Could not read input action map.");
    }
    text[length] = '\0';
    cJSON *root = cJSON_Parse(text);
    free(text);
    cJSON *schema = root ? cJSON_GetObjectItemCaseSensitive(root, "schemaVersion") : NULL;
    cJSON *actions = root ? cJSON_GetObjectItemCaseSensitive(root, "actions") : NULL;
    if (!cJSON_IsObject(root) || !cJSON_IsNumber(schema) || schema->valueint != 1 ||
        !cJSON_IsArray(actions) || cJSON_GetArraySize(actions) > BINPUT_MAX_ACTIONS) {
        cJSON_Delete(root);
        return Fail(error, errorSize,
                    "Input action map must use schemaVersion 1 and a bounded actions array.");
    }
    BInputAction parsed[BINPUT_MAX_ACTIONS] = {0};
    int count = 0;
    bool valid = true;
    for (cJSON *item = actions->child; item && valid; item = item->next) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *device = cJSON_GetObjectItemCaseSensitive(item, "device");
        cJSON *code = cJSON_GetObjectItemCaseSensitive(item, "code");
        valid = cJSON_IsObject(item) && cJSON_IsString(name) && name->valuestring[0] &&
                strlen(name->valuestring) < BINPUT_ACTION_NAME_MAX && cJSON_IsString(device) &&
                cJSON_IsNumber(code) && code->valuedouble == code->valueint &&
                code->valueint >= 0 &&
                (!strcmp(device->valuestring, "keyboard") || !strcmp(device->valuestring, "mouse"));
        for (int i = 0; i < count && valid; ++i)
            if (!strcmp(parsed[i].name, name->valuestring))
                valid = false;
        if (valid) {
            snprintf(parsed[count].name, sizeof(parsed[count].name), "%s", name->valuestring);
            parsed[count].code = code->valueint;
            parsed[count].device = !strcmp(device->valuestring, "mouse") ? BINPUT_DEVICE_MOUSE
                                                                         : BINPUT_DEVICE_KEYBOARD;
            ++count;
        }
    }
    cJSON_Delete(root);
    if (!valid)
        return Fail(error, errorSize, "Input action entry is invalid or duplicated.");
    memcpy(g_Actions, parsed, sizeof(parsed));
    g_ActionCount = count;
    return true;
}
