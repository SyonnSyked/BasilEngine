#include <stdbool.h>
#include <stddef.h>

#include <raylib.h>

#include "../../engine/core/BApplication.h"
#include "../../engine/rendering/AsciiCanvas.h"

typedef struct WhereBirdsNestGame
{
    AsciiCanvas canvas;
} WhereBirdsNestGame;

static bool WhereBirdsNest_OnStart(void* userData, BEngine* engine)
{
    (void)engine;

    WhereBirdsNestGame* game = (WhereBirdsNestGame*)userData;

    if (game == 0)
        return false;

    const char* cabinArt[] =
    {
        "        *                 *          ",
        "              /\\                    ",
        "             /  \\       *            ",
        "    *       /____\\                   ",
        "           | [] [] |                 ",
        "           |  __   |        *        ",
        "    --- ---| |  |  |--- ---          ",
        "     . . . |_|__|__| . . .           "
    };

    if (!AsciiCanvas_Init(&game->canvas, 40, 12, 1, 80, 80, 18, 1, RAYWHITE))
        return false;

    return AsciiCanvas_LoadLayerFromText(
        &game->canvas,
        cabinArt,
        sizeof(cabinArt) / sizeof(cabinArt[0]),
        0
    );
}

static void WhereBirdsNest_OnUpdate(void* userData, BEngine* engine, float deltaTime)
{
    (void)userData;
    (void)engine;
    (void)deltaTime;
}

static void WhereBirdsNest_OnRender(void* userData, BEngine* engine)
{
    (void)engine;

    WhereBirdsNestGame* game = (WhereBirdsNestGame*)userData;

    if (game == 0)
        return;

    ClearBackground(BLACK);
    AsciiCanvas_Draw(&game->canvas);
}

static void WhereBirdsNest_OnShutdown(void* userData, BEngine* engine)
{
    (void)engine;

    WhereBirdsNestGame* game = (WhereBirdsNestGame*)userData;

    if (game == 0)
        return;

    AsciiCanvas_Destroy(&game->canvas);
}

int main(void)
{
    WhereBirdsNestGame game;

    BEngineConfig config = BEngineConfig_Default();
    config.windowWidth = 800;
    config.windowHeight = 450;
    config.windowTitle = "Where Birds Nest - BasilEngine";
    config.targetFPS = 60;

    BApplicationCallbacks callbacks;
    callbacks.onStart = WhereBirdsNest_OnStart;
    callbacks.onUpdate = WhereBirdsNest_OnUpdate;
    callbacks.onRender = WhereBirdsNest_OnRender;
    callbacks.onShutdown = WhereBirdsNest_OnShutdown;
    callbacks.userData = &game;

    BApplication app;

    if (!BApplication_Init(&app, config, callbacks))
        return 1;

    return BApplication_Run(&app);
}

