#include "BGeneratedRuntime.h"

#include "BApplication.h"
#include "BAsciiDrawList.h"
#include "BDynamicLibrary.h"
#include "BGameModule.h"
#include "BInput.h"
#include "BLog.h"
#include "BProjectContext.h"

#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BGeneratedRuntimeState {
    BProjectContext context;
    BWorkspaceDocument document;
    uint32_t workspaceGeneration;
    bool workspaceRequestPending;
    char requestedWorkspace[BPROJECT_PATH_MAX];
    BTextSpriteCache cache;
    BAsciiDrawList drawList;
    BDiagnosticList diagnostics;
    bool loaded;
    bool drawDirty;
    BDynamicLibrary library;
    BGameHostAPI hostAPI;
    BGameModule gameModule;
    void *gameState;
    bool moduleInitialized;
    char errorMessage[BDIAGNOSTIC_MESSAGE_MAX];
} BGeneratedRuntimeState;

static BGameEntity Runtime_MakeEntityHandle(const BGeneratedRuntimeState *state, size_t index)
{
    if (state == NULL || index >= state->document.entityCount || index >= UINT32_MAX) {
        return (BGameEntity){0};
    }

    uint64_t value = ((uint64_t)state->workspaceGeneration << 32) | (uint64_t)(index + 1);

    return (BGameEntity){value};
}

static BWorkspaceEntity *Runtime_Entity(BGeneratedRuntimeState *state, BGameEntity handle)
{
    if (state == NULL || handle.value == 0)
        return NULL;

    uint32_t generation = (uint32_t)(handle.value >> 32);

    uint32_t encodedIndex = (uint32_t)(handle.value & UINT32_MAX);

    if (generation != state->workspaceGeneration || encodedIndex == 0) {
        return NULL;
    }

    size_t index = (size_t)(encodedIndex - 1);

    if (index >= state->document.entityCount)
        return NULL;

    return &state->document.entities[index];
}

static void Host_Log(void *context, const char *message)
{
    (void)context;
    if (message)
        BLog_Info(message);
}
static const char *Host_ProjectRoot(void *context)
{
    return ((BGeneratedRuntimeState *)context)->context.projectRoot;
}
static size_t Host_EntityCount(void *context)
{
    return ((BGeneratedRuntimeState *)context)->document.entityCount;
}

static BGameEntity Host_EntityAt(void *context, size_t index)
{
    BGeneratedRuntimeState *state = context;

    return Runtime_MakeEntityHandle(state, index);
}

static const char *Host_EntityId(void *context, BGameEntity entity)
{
    BWorkspaceEntity *value = Runtime_Entity(context, entity);
    return value ? value->id : NULL;
}
static const char *Host_EntityName(void *context, BGameEntity entity)
{
    BWorkspaceEntity *value = Runtime_Entity(context, entity);
    return value ? value->name : NULL;
}
static bool Host_GetPosition(void *context, BGameEntity entity, float *x, float *y)
{
    BWorkspaceEntity *value = Runtime_Entity(context, entity);
    BWorkspaceComponent *component =
        value ? BWorkspaceEntity_FindComponent(value, BWORKSPACE_TRANSFORM2D_TYPE) : NULL;
    if (!component || !x || !y)
        return false;
    *x = component->data.transform2d.x;
    *y = component->data.transform2d.y;
    return true;
}
static bool Host_SetPosition(void *context, BGameEntity entity, float x, float y)
{
    BGeneratedRuntimeState *state = context;
    BWorkspaceEntity *value = Runtime_Entity(state, entity);
    if (!value)
        return false;
    size_t index = (size_t)(value - state->document.entities);
    BDiagnosticList diagnostics = {0};
    if (!BWorkspaceDocument_SetTransform2D(&state->document, index, (BTransform2D){x, y},
                                           &diagnostics))
        return false;
    state->drawDirty = true;
    return true;
}
static const char *Host_ComponentJson(void *context, BGameEntity entity, const char *type)
{
    BWorkspaceEntity *value = Runtime_Entity(context, entity);
    BWorkspaceComponent *component = value ? BWorkspaceEntity_FindComponent(value, type) : NULL;
    return component && component->kind == BWORKSPACE_COMPONENT_UNKNOWN
               ? component->data.unknownDataJson
               : NULL;
}

static bool Host_InputPressed(void *context, const char *action)
{
    (void)context;
    return BInput_IsActionPressed(action);
}

static bool Host_InputDown(void *context, const char *action)
{
    (void)context;
    return BInput_IsActionDown(action);
}

static bool Host_InputReleased(void *context, const char *action)
{
    (void)context;
    return BInput_IsActionReleased(action);
}

static bool Host_InputRebindKeyboard(void *context, const char *action, int key)
{
    (void)context;
    return BInput_RebindAction(action, key);
}

static bool Host_InputRebindMouse(void *context, const char *action, int button)
{
    (void)context;
    return BInput_RebindMouseAction(action, button);
}

static bool Host_InputHasAction(void *context, const char *action)
{
    (void)context;
    return BInput_HasAction(action);
}

static int Host_InputBindingCode(void *context, const char *action)
{
    (void)context;
    return BInput_GetActionCode(action);
}

static int Host_InputBindingDevice(void *context, const char *action)
{
    (void)context;
    return (int)BInput_GetActionDevice(action);
}

static bool Runtime_ProcessWorkspaceRequest(BGeneratedRuntimeState *state)
{
    if (state == NULL || !state->workspaceRequestPending) {
        return true;
    }

    char requestedWorkspace[BPROJECT_PATH_MAX];

    snprintf(requestedWorkspace, sizeof(requestedWorkspace), "%s", state->requestedWorkspace);

    state->workspaceRequestPending = false;
    state->requestedWorkspace[0] = '\0';

    if (state->workspaceGeneration == UINT32_MAX) {
        BLog_Error("Workspace generation limit reached; "
                   "Workspace replacement was rejected.");

        return false;
    }

    char resolvedPath[BPROJECT_PATH_MAX];
    BDiagnosticList diagnostics = {0};

    if (!BProjectContext_ResolvePath(&state->context, requestedWorkspace, resolvedPath,
                                     sizeof(resolvedPath), &diagnostics)) {

        const BDiagnostic *error = BDiagnosticList_FirstError(&diagnostics);

        BLog_Error(error != NULL && error->message[0] != '\0'
                       ? error->message
                       : "Workspace path resolution failed.");

        return false;
    }

    BWorkspaceDocument replacementDocument;
    BWorkspaceDocument_Init(&replacementDocument);

    BAsciiDrawList replacementDrawList;
    BAsciiDrawList_Init(&replacementDrawList);

    bool succeeded = false;

    if (!BWorkspaceDocument_Load(resolvedPath, &replacementDocument, &diagnostics)) {

        const BDiagnostic *error = BDiagnosticList_FirstError(&diagnostics);

        BLog_Error(error != NULL && error->message[0] != '\0' ? error->message
                                                              : "Workspace loading failed.");

        goto cleanup;
    }

    if (!BAsciiDrawList_Build(&replacementDocument, state->context.projectRoot, &state->cache,
                              &replacementDrawList, &diagnostics)) {

        const BDiagnostic *error = BDiagnosticList_FirstError(&diagnostics);

        BLog_Error(error != NULL && error->message[0] != '\0'
                       ? error->message
                       : "Workspace draw-list construction failed.");

        goto cleanup;
    }

    BWorkspaceDocument_Swap(&state->document, &replacementDocument);

    BAsciiDrawList_Swap(&state->drawList, &replacementDrawList);

    snprintf(state->context.workspacePath, sizeof(state->context.workspacePath), "%s",
             resolvedPath);

    ++state->workspaceGeneration;

    state->drawDirty = false;

    BLog_Info("Workspace replacement completed.");

    succeeded = true;

cleanup:

    BAsciiDrawList_Destroy(&replacementDrawList);

    BWorkspaceDocument_Destroy(&replacementDocument);

    return succeeded;
}

static bool Runtime_ModulePath(int argumentCount, char **arguments, const char *identifier,
                               char *output, size_t outputSize)
{
    if (argumentCount < 1 || !arguments[0])
        return false;
    char executable[BDIAGNOSTIC_PATH_MAX];
#ifdef _WIN32
    if (_fullpath(executable, arguments[0], sizeof(executable)) == NULL)
        return false;
    const char *extension = ".game.dll";
#elif defined(__APPLE__)
    if (realpath(arguments[0], executable) == NULL)
        return false;
    const char *extension = ".game.dylib";
#else
    if (realpath(arguments[0], executable) == NULL)
        return false;
    const char *extension = ".game.so";
#endif
    char *slash = strrchr(executable, '/');
    char *backslash = strrchr(executable, '\\');
    if (!slash || (backslash && backslash > slash))
        slash = backslash;
    if (!slash)
        return false;
    *slash = '\0';
    int written = snprintf(output, outputSize, "%s/%s%s", executable, identifier, extension);
    return written > 0 && (size_t)written < outputSize;
}

static bool Host_RequestWorkspace(void *context, const char *workspacePath)
{
    BGeneratedRuntimeState *state = (BGeneratedRuntimeState *)context;

    if (state == NULL || workspacePath == NULL || workspacePath[0] == '\0') {
        return false;
    }

    if (state->workspaceRequestPending)
        return false;

    size_t length = strlen(workspacePath);

    if (length >= sizeof(state->requestedWorkspace))
        return false;

    memcpy(state->requestedWorkspace, workspacePath, length + 1);

    state->workspaceRequestPending = true;

    return true;
}

static uint32_t Host_WorkspaceGeneration(void *context)
{
    BGeneratedRuntimeState *state = (BGeneratedRuntimeState *)context;

    return state != NULL ? state->workspaceGeneration : 0;
}

static bool Runtime_LoadModule(BGeneratedRuntimeState *state, int argc, char **argv)
{
    char path[BDIAGNOSTIC_PATH_MAX];
    char detail[BDIAGNOSTIC_MESSAGE_MAX];
    if (!Runtime_ModulePath(argc, argv, state->context.project.identifier, path, sizeof(path))) {
        snprintf(state->errorMessage, sizeof(state->errorMessage),
                 "Could not resolve the game module path.");
        return false;
    }
    if (!BDynamicLibrary_Open(&state->library, path, detail, sizeof(detail))) {
        snprintf(state->errorMessage, sizeof(state->errorMessage), "%s", detail);
        return false;
    }
    void *symbol =
        BDynamicLibrary_Symbol(&state->library, BGAME_MODULE_QUERY_NAME, detail, sizeof(detail));
    BGameModuleQueryFn query = 0;
    memcpy(&query, &symbol, sizeof(query));
    if (!query) {
        snprintf(state->errorMessage, sizeof(state->errorMessage), "%s", detail);
        return false;
    }
    memset(&state->gameModule, 0, sizeof(state->gameModule));
    if (!query(BGAME_API_VERSION, &state->gameModule) ||
        state->gameModule.version != BGAME_API_VERSION ||
        state->gameModule.structSize < sizeof(BGameModule)) {
        snprintf(state->errorMessage, sizeof(state->errorMessage),
                 "Game module API mismatch: host requires %u, module provided %u.",
                 BGAME_API_VERSION, state->gameModule.version);
        return false;
    }
    state->hostAPI = (BGameHostAPI){.version = BGAME_API_VERSION,
                                    .structSize = sizeof(BGameHostAPI),
                                    .context = state,

                                    .log = Host_Log,
                                    .projectRoot = Host_ProjectRoot,

                                    .entityCount = Host_EntityCount,
                                    .entityAt = Host_EntityAt,
                                    .entityId = Host_EntityId,
                                    .entityName = Host_EntityName,

                                    .getPosition = Host_GetPosition,
                                    .setPosition = Host_SetPosition,
                                    .componentJson = Host_ComponentJson,

                                    .inputPressed = Host_InputPressed,
                                    .inputDown = Host_InputDown,
                                    .inputReleased = Host_InputReleased,

                                    .inputRebindKeyboard = Host_InputRebindKeyboard,
                                    .inputRebindMouse = Host_InputRebindMouse,

                                    .inputHasAction = Host_InputHasAction,
                                    .inputBindingCode = Host_InputBindingCode,
                                    .inputBindingDevice = Host_InputBindingDevice,
                                    .requestWorkspace = Host_RequestWorkspace,
                                    .workspaceGeneration = Host_WorkspaceGeneration};

    return true;
}

static Color Runtime_Color(BAsciiColor color)
{
    return (Color){color.r, color.g, color.b, color.a};
}

static bool Runtime_OnStart(void *userData, BEngine *engine)
{
    (void)engine;

    BGeneratedRuntimeState *state = userData;

    if (!state->loaded)
        return true;

    char path[BPROJECT_PATH_MAX + 32];
    char error[BDIAGNOSTIC_MESSAGE_MAX];

    int written = snprintf(path, sizeof(path), "%s/.basil/input.json", state->context.projectRoot);

    if (written < 0 || (size_t)written >= sizeof(path)) {
        snprintf(state->errorMessage, sizeof(state->errorMessage),
                 "Input action map path is too long.");

        BLog_Error(state->errorMessage);
        state->loaded = false;
        return true;
    }

    if (!BInput_LoadActionMap(path, error, sizeof(error))) {

        snprintf(state->errorMessage, sizeof(state->errorMessage), "%s", error);

        BLog_Error(state->errorMessage);
        state->loaded = false;
        return true;
    }

    state->gameState = NULL;

    if (state->gameModule.onInitialize &&
        !state->gameModule.onInitialize(&state->hostAPI, &state->gameState)) {

        snprintf(state->errorMessage, sizeof(state->errorMessage),
                 "Game module initialization failed.");

        BLog_Error(state->errorMessage);
        state->loaded = false;
        return true;
    }

    state->moduleInitialized = true;

    return true;
}

static void Runtime_OnUpdate(void *userData, BEngine *engine, float deltaTime)
{
    (void)engine;
    BGeneratedRuntimeState *state = userData;
    BInput_SetFocusSuppressed(!IsWindowFocused());
    if (state->moduleInitialized && state->gameModule.onUpdate)
        state->gameModule.onUpdate(state->gameState, deltaTime);

    if (state->workspaceRequestPending)
        Runtime_ProcessWorkspaceRequest(state);

    if (state->drawDirty) {
        BAsciiDrawList replacement;
        BAsciiDrawList_Init(&replacement);
        BDiagnosticList diagnostics = {0};
        if (BAsciiDrawList_Build(&state->document, state->context.projectRoot, &state->cache,
                                 &replacement, &diagnostics)) {
            BAsciiDrawList_Swap(&state->drawList, &replacement);
            state->drawDirty = false;
        }
        BAsciiDrawList_Destroy(&replacement);
    }
}

static void Runtime_OnRender(void *userData, BEngine *engine)
{
    (void)engine;
    BGeneratedRuntimeState *state = (BGeneratedRuntimeState *)userData;
    const Color background = {5, 9, 14, 255};
    const Color cyan = {0, 229, 255, 255};
    const Color muted = {119, 142, 153, 255};
    ClearBackground(background);
    if (state->moduleInitialized && state->gameModule.onRender)
        state->gameModule.onRender(state->gameState);

    if (!state->loaded) {
        DrawText("PROJECT LOAD FAILED", 32, 32, 24, (Color){255, 82, 122, 255});
        DrawText(state->errorMessage, 32, 72, 18, RAYWHITE);
        DrawText("Close this window after reviewing the diagnostic.", 32, 108, 16, muted);
        return;
    }

    if (state->drawList.count == 0) {
        DrawText("WORKSPACE ONLINE", 32, 32, 24, cyan);
        DrawText(state->context.project.startupWorkspace, 32, 70, 18, RAYWHITE);
        DrawText("0 renderable entities", 32, 100, 16, muted);
        return;
    }

    const int cellWidth = 16;
    const int cellHeight = 24;
    const float originX = (float)GetScreenWidth() * 0.5f;
    const float originY = (float)GetScreenHeight() * 0.5f;
    for (size_t i = 0; i < state->drawList.count; ++i) {
        const BAsciiDrawItem *item = &state->drawList.items[i];
        int x = (int)(originX + item->x * (float)cellWidth);
        int y = (int)(originY + item->y * (float)cellHeight);
        Color cellBackground = Runtime_Color(item->background);
        if (cellBackground.a > 0)
            DrawRectangle(x, y, cellWidth, cellHeight, cellBackground);
        char text[2] = {item->glyph, '\0'};
        DrawText(text, x, y, 24, Runtime_Color(item->foreground));
    }
}

static void Runtime_OnShutdown(void *userData, BEngine *engine)
{
    (void)engine;
    BGeneratedRuntimeState *state = userData;
    if (state->moduleInitialized && state->gameModule.onShutdown)
        state->gameModule.onShutdown(state->gameState);
    state->moduleInitialized = false;
}

static bool HasArgument(int count, char **values, const char *expected)
{
    for (int i = 1; i < count; ++i)
        if (strcmp(values[i], expected) == 0)
            return true;
    return false;
}

int BGeneratedRuntime_Run(int argumentCount, char **arguments, const char *fallbackTitle)
{
    BGeneratedRuntimeState state;
    memset(&state, 0, sizeof(state));
    BProjectContext_Init(&state.context);
    BWorkspaceDocument_Init(&state.document);
    state.workspaceGeneration = 1;
    BTextSpriteCache_Init(&state.cache);
    BAsciiDrawList_Init(&state.drawList);

    state.loaded =
        BProjectContext_Discover(argumentCount, arguments, &state.context, &state.diagnostics) &&
        BWorkspaceDocument_Load(state.context.workspacePath, &state.document, &state.diagnostics) &&
        BAsciiDrawList_Build(&state.document, state.context.projectRoot, &state.cache,
                             &state.drawList, &state.diagnostics);

    const BDiagnostic *error = BDiagnosticList_FirstError(&state.diagnostics);
    if (!state.loaded) {
        snprintf(state.errorMessage, sizeof(state.errorMessage), "%s",
                 error != NULL ? error->message : "Unknown Project load failure.");
        fprintf(stderr, "BASIL_RUNTIME_ERROR: %s", state.errorMessage);
        if (error != NULL && error->path[0] != '\0')
            fprintf(stderr, " [%s]", error->path);
        fprintf(stderr, "\n");
    } else if (!Runtime_LoadModule(&state, argumentCount, arguments)) {
        state.loaded = false;
        fprintf(stderr, "BASIL_RUNTIME_ERROR: %s\n", state.errorMessage);
    }

    if (HasArgument(argumentCount, arguments, "--basil-validate")) {
        if (state.loaded) {
            printf("BASIL_RUNTIME_READY project=%s workspace=%s items=%zu\n",
                   state.context.project.identifier, state.context.project.startupWorkspace,
                   state.drawList.count);

            fflush(stdout);
        }

        int result = state.loaded ? 0 : 1;

        Runtime_OnShutdown(&state, NULL);
        BDynamicLibrary_Close(&state.library);
        BAsciiDrawList_Destroy(&state.drawList);
        BTextSpriteCache_Destroy(&state.cache);
        BWorkspaceDocument_Destroy(&state.document);
        BProjectContext_Destroy(&state.context);

        return result;
    }
    BEngineConfig config = BEngineConfig_Default();
    config.windowConfig.title =
        state.loaded ? state.context.project.name
                     : (fallbackTitle != NULL ? fallbackTitle : "BasilEngine Project Error");
    BApplicationCallbacks callbacks = {0};
    callbacks.onStart = Runtime_OnStart;
    callbacks.onUpdate = Runtime_OnUpdate;
    callbacks.onRender = Runtime_OnRender;
    callbacks.onShutdown = Runtime_OnShutdown;
    callbacks.userData = &state;
    BApplication application;
    int result = 1;
    if (BApplication_Init(&application, config, callbacks)) {
        if (state.loaded) {
            printf("BASIL_RUNTIME_READY project=%s workspace=%s items=%zu\n",
                   state.context.project.identifier, state.context.project.startupWorkspace,
                   state.drawList.count);

            fflush(stdout);
        }

        result = BApplication_Run(&application);

        if (!state.loaded)
            result = 1;
    }
    BAsciiDrawList_Destroy(&state.drawList);
    BTextSpriteCache_Destroy(&state.cache);
    BWorkspaceDocument_Destroy(&state.document);
    BProjectContext_Destroy(&state.context);
    BDynamicLibrary_Close(&state.library);
    return result;
}
