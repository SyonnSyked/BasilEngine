#include "BGeneratedRuntime.h"

#include "BApplication.h"
#include "BAsciiDrawList.h"
#include "BProjectContext.h"

#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct BGeneratedRuntimeState
{
    BProjectContext context;
    BWorkspaceDocument document;
    BTextSpriteCache cache;
    BAsciiDrawList drawList;
    BDiagnosticList diagnostics;
    bool loaded;
    char errorMessage[BDIAGNOSTIC_MESSAGE_MAX];
} BGeneratedRuntimeState;

static Color Runtime_Color(BAsciiColor color)
{
    return (Color){ color.r, color.g, color.b, color.a };
}

static bool Runtime_OnStart(void* userData, BEngine* engine)
{
    (void)userData;
    (void)engine;
    return true;
}

static void Runtime_OnUpdate(void* userData, BEngine* engine, float deltaTime)
{
    (void)userData;
    (void)engine;
    (void)deltaTime;
}

static void Runtime_OnRender(void* userData, BEngine* engine)
{
    (void)engine;
    BGeneratedRuntimeState* state = (BGeneratedRuntimeState*)userData;
    const Color background = { 5, 9, 14, 255 };
    const Color cyan = { 0, 229, 255, 255 };
    const Color muted = { 119, 142, 153, 255 };
    ClearBackground(background);

    if (!state->loaded)
    {
        DrawText("PROJECT LOAD FAILED", 32, 32, 24, (Color){ 255, 82, 122, 255 });
        DrawText(state->errorMessage, 32, 72, 18, RAYWHITE);
        DrawText("Close this window after reviewing the diagnostic.", 32, 108, 16, muted);
        return;
    }

    if (state->drawList.count == 0)
    {
        DrawText("WORKSPACE ONLINE", 32, 32, 24, cyan);
        DrawText(state->context.project.startupWorkspace, 32, 70, 18, RAYWHITE);
        DrawText("0 renderable entities", 32, 100, 16, muted);
        return;
    }

    const int cellWidth = 16;
    const int cellHeight = 24;
    const float originX = (float)GetScreenWidth() * 0.5f;
    const float originY = (float)GetScreenHeight() * 0.5f;
    for (size_t i = 0; i < state->drawList.count; ++i)
    {
        const BAsciiDrawItem* item = &state->drawList.items[i];
        int x = (int)(originX + item->x * (float)cellWidth);
        int y = (int)(originY + item->y * (float)cellHeight);
        Color cellBackground = Runtime_Color(item->background);
        if (cellBackground.a > 0)
            DrawRectangle(x, y, cellWidth, cellHeight, cellBackground);
        char text[2] = { item->glyph, '\0' };
        DrawText(text, x, y, 24, Runtime_Color(item->foreground));
    }
}

static void Runtime_OnShutdown(void* userData, BEngine* engine)
{
    (void)userData;
    (void)engine;
}

static bool HasArgument(int count, char** values, const char* expected)
{
    for (int i = 1; i < count; ++i)
        if (strcmp(values[i], expected) == 0)
            return true;
    return false;
}

int BGeneratedRuntime_Run(int argumentCount, char** arguments, const char* fallbackTitle)
{
    BGeneratedRuntimeState state;
    memset(&state, 0, sizeof(state));
    BProjectContext_Init(&state.context);
    BWorkspaceDocument_Init(&state.document);
    BTextSpriteCache_Init(&state.cache);
    BAsciiDrawList_Init(&state.drawList);

    state.loaded = BProjectContext_Discover(argumentCount, arguments, &state.context, &state.diagnostics) &&
        BWorkspaceDocument_Load(state.context.workspacePath, &state.document, &state.diagnostics) &&
        BAsciiDrawList_Build(&state.document, state.context.projectRoot, &state.cache, &state.drawList, &state.diagnostics);

    const BDiagnostic* error = BDiagnosticList_FirstError(&state.diagnostics);
    if (!state.loaded)
    {
        snprintf(state.errorMessage, sizeof(state.errorMessage), "%s", error != NULL ? error->message : "Unknown Project load failure.");
        fprintf(stderr, "BASIL_RUNTIME_ERROR: %s", state.errorMessage);
        if (error != NULL && error->path[0] != '\0')
            fprintf(stderr, " [%s]", error->path);
        fprintf(stderr, "\n");
    }
    else
    {
        printf(
            "BASIL_RUNTIME_READY project=%s workspace=%s items=%zu\n",
            state.context.project.identifier,
            state.context.project.startupWorkspace,
            state.drawList.count
        );
    }

    if (HasArgument(argumentCount, arguments, "--basil-validate"))
    {
        int result = state.loaded ? 0 : 1;
        BAsciiDrawList_Destroy(&state.drawList);
        BTextSpriteCache_Destroy(&state.cache);
        BWorkspaceDocument_Destroy(&state.document);
        BProjectContext_Destroy(&state.context);
        return result;
    }

    BEngineConfig config = BEngineConfig_Default();
    config.windowConfig.title = state.loaded ? state.context.project.name :
        (fallbackTitle != NULL ? fallbackTitle : "BasilEngine Project Error");
    BApplicationCallbacks callbacks = { 0 };
    callbacks.onStart = Runtime_OnStart;
    callbacks.onUpdate = Runtime_OnUpdate;
    callbacks.onRender = Runtime_OnRender;
    callbacks.onShutdown = Runtime_OnShutdown;
    callbacks.userData = &state;
    BApplication application;
    int result = 1;
    if (BApplication_Init(&application, config, callbacks))
    {
        result = BApplication_Run(&application);
        if (!state.loaded)
            result = 1;
    }

    BAsciiDrawList_Destroy(&state.drawList);
    BTextSpriteCache_Destroy(&state.cache);
    BWorkspaceDocument_Destroy(&state.document);
    BProjectContext_Destroy(&state.context);
    return result;
}
