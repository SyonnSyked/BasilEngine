#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <raylib.h>

#include "../../engine/core/BApplication.h"
#include "../../engine/debug/BConsole.h"
#include "../../engine/input/BInput.h"
#include "../../engine/rendering/AsciiCanvas.h"

#define ARENA_WIDTH 48
#define ARENA_HEIGHT 24
#define PLAYER_RADIUS 0.28f
#define PLAYER_SPEED 7.0f

typedef struct WhereBirdsNestGame
{
    AsciiCanvas canvas;
    Vector2 playerPosition;
    bool canvasInitialized;
} WhereBirdsNestGame;

static bool WhereBirdsNest_IsBlocked(
    const WhereBirdsNestGame* game,
    float x,
    float y
)
{
    int cellX = (int)floorf(x);
    int cellY = (int)floorf(y);

    if (cellX < 0 || cellY < 0 || cellX >= ARENA_WIDTH || cellY >= ARENA_HEIGHT)
        return true;

    int character = 0;

    if (!AsciiCanvas_GetCharacter(
        &game->canvas,
        (size_t)cellX,
        (size_t)cellY,
        0,
        &character,
        0))
    {
        return true;
    }

    return character == '#';
}

static bool WhereBirdsNest_CanOccupy(
    const WhereBirdsNestGame* game,
    Vector2 position
)
{
    return
        !WhereBirdsNest_IsBlocked(game, position.x - PLAYER_RADIUS, position.y - PLAYER_RADIUS) &&
        !WhereBirdsNest_IsBlocked(game, position.x + PLAYER_RADIUS, position.y - PLAYER_RADIUS) &&
        !WhereBirdsNest_IsBlocked(game, position.x - PLAYER_RADIUS, position.y + PLAYER_RADIUS) &&
        !WhereBirdsNest_IsBlocked(game, position.x + PLAYER_RADIUS, position.y + PLAYER_RADIUS);
}

static bool WhereBirdsNest_OnStart(void* userData, BEngine* engine)
{
    (void)engine;

    WhereBirdsNestGame* game = (WhereBirdsNestGame*)userData;

    if (game == 0)
        return false;

    if (!AsciiCanvas_Init(
        &game->canvas,
        ARENA_WIDTH,
        ARENA_HEIGHT,
        1,
        0,
        0,
        18,
        1,
        DARKGREEN))
    {
        return false;
    }

    game->canvasInitialized = true;

    char assetPath[1024];
    snprintf(
        assetPath,
        sizeof(assetPath),
        "%sassets/wherebirdsnest/arena.txt",
        GetApplicationDirectory()
    );

    if (!AsciiCanvas_LoadLayerFromFile(&game->canvas, assetPath, 0))
        return false;

    game->playerPosition = (Vector2){ 2.5f, 2.5f };
    return true;
}

static void WhereBirdsNest_OnUpdate(void* userData, BEngine* engine, float deltaTime)
{
    (void)engine;

    WhereBirdsNestGame* game = (WhereBirdsNestGame*)userData;

    if (game == 0 || BConsole_IsOpen())
        return;

    Vector2 direction = { 0.0f, 0.0f };

    if (BInput_IsActionDown("move_left"))
        direction.x -= 1.0f;
    if (BInput_IsActionDown("move_right"))
        direction.x += 1.0f;
    if (BInput_IsActionDown("move_up"))
        direction.y -= 1.0f;
    if (BInput_IsActionDown("move_down"))
        direction.y += 1.0f;

    float length = sqrtf(direction.x * direction.x + direction.y * direction.y);

    if (length > 0.0f)
    {
        direction.x /= length;
        direction.y /= length;
    }

    Vector2 nextPosition = game->playerPosition;
    nextPosition.x += direction.x * PLAYER_SPEED * deltaTime;

    if (WhereBirdsNest_CanOccupy(game, nextPosition))
        game->playerPosition.x = nextPosition.x;

    nextPosition = game->playerPosition;
    nextPosition.y += direction.y * PLAYER_SPEED * deltaTime;

    if (WhereBirdsNest_CanOccupy(game, nextPosition))
        game->playerPosition.y = nextPosition.y;

    Vector2 cellSize = AsciiCanvas_GetCellSize(&game->canvas);
    int originX = GetScreenWidth() / 2 - (int)(game->playerPosition.x * cellSize.x);
    int originY = GetScreenHeight() / 2 - (int)(game->playerPosition.y * cellSize.y);
    AsciiCanvas_SetOrigin(&game->canvas, originX, originY);
}

static void WhereBirdsNest_OnRender(void* userData, BEngine* engine)
{
    (void)engine;

    WhereBirdsNestGame* game = (WhereBirdsNestGame*)userData;

    if (game == 0)
        return;

    ClearBackground((Color){ 9, 12, 10, 255 });
    AsciiCanvas_Draw(&game->canvas);
    AsciiCanvas_DrawCharacter(&game->canvas, '@', game->playerPosition, GOLD);
    DrawText("WASD move  |  ` console", 16, 16, 18, RAYWHITE);
}

static void WhereBirdsNest_OnShutdown(void* userData, BEngine* engine)
{
    (void)engine;

    WhereBirdsNestGame* game = (WhereBirdsNestGame*)userData;

    if (game == 0)
        return;

    if (game->canvasInitialized)
    {
        AsciiCanvas_Destroy(&game->canvas);
        game->canvasInitialized = false;
    }
}

int main(void)
{
    WhereBirdsNestGame game = { 0 };

    BEngineConfig config = BEngineConfig_Default();
    config.windowConfig.width = 1280;
    config.windowConfig.height = 720;
    config.windowConfig.title = "Where Birds Nest - Movement Feasibility";
    config.windowConfig.targetFPS = 60;

    BApplicationCallbacks callbacks = { 0 };
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
