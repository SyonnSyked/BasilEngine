#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <raylib.h>

#include "../../engine/core/BApplication.h"
#include "../../engine/debug/BConsole.h"
#include "../../engine/input/BInput.h"
#include "../../engine/rendering/AsciiCanvas.h"
#include "WBNCombat.h"

#define ARENA_WIDTH 48
#define ARENA_HEIGHT 24
#define PLAYER_RADIUS 0.45f
#define PLAYER_SPEED 8.5f
#define BASIC_ATTACK_RANGE 3.5f
#define BASIC_ATTACK_DAMAGE 1
#define BASIC_ATTACK_COOLDOWN 0.3f
#define ATTACK_EFFECT_DURATION 0.12f

typedef struct WhereBirdsNestGame
{
    AsciiCanvas canvas;
    Vector2 playerPosition;
    WBNCombatTarget target;
    WBNBasicAttackResult lastAttackResult;
    float attackCooldown;
    float attackEffectTime;
    float attackMessageTime;
    bool canvasInitialized;
} WhereBirdsNestGame;

static void WhereBirdsNest_ApplyArenaPalette(WhereBirdsNestGame* game)
{
    const Color wallForeground = (Color){ 191, 149, 91, 255 };
    const Color wallBackground = (Color){ 45, 30, 22, 255 };
    const Color floorForeground = (Color){ 76, 67, 52, 255 };
    const Color floorBackground = (Color){ 18, 20, 17, 255 };

    for (size_t y = 0; y < ARENA_HEIGHT; ++y)
    {
        for (size_t x = 0; x < ARENA_WIDTH; ++x)
        {
            int character = 0;

            if (!AsciiCanvas_GetCharacter(&game->canvas, x, y, 0, &character, 0))
                continue;

            if (character == '#')
            {
                AsciiCanvas_SetCellColors(
                    &game->canvas,
                    x,
                    y,
                    0,
                    wallForeground,
                    wallBackground
                );
            }
            else if (character == '.')
            {
                AsciiCanvas_SetCellColors(
                    &game->canvas,
                    x,
                    y,
                    0,
                    floorForeground,
                    floorBackground
                );
            }
        }
    }
}

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

    WhereBirdsNest_ApplyArenaPalette(game);
    game->playerPosition = (Vector2){ 2.5f, 2.5f };

    if (!BInput_RegisterAction("basic_attack", KEY_SPACE))
        return false;

    WBNCombatTarget_Init(
        &game->target,
        (WBNPosition){ 12.5f, 2.5f },
        3
    );
    game->attackCooldown = 0.0f;
    game->attackEffectTime = 0.0f;
    game->attackMessageTime = 0.0f;
    game->lastAttackResult = WBN_ATTACK_INVALID;
    return true;
}

static void WhereBirdsNest_OnUpdate(void* userData, BEngine* engine, float deltaTime)
{
    (void)engine;

    WhereBirdsNestGame* game = (WhereBirdsNestGame*)userData;

    if (game == 0 || BConsole_IsOpen())
        return;

    if (game->attackCooldown > 0.0f)
        game->attackCooldown -= deltaTime;

    if (game->attackEffectTime > 0.0f)
        game->attackEffectTime -= deltaTime;

    if (game->attackMessageTime > 0.0f)
        game->attackMessageTime -= deltaTime;

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

    if (BInput_IsActionPressed("basic_attack") && game->attackCooldown <= 0.0f)
    {
        game->lastAttackResult = WBNCombat_TryBasicAttack(
            (WBNPosition){ game->playerPosition.x, game->playerPosition.y },
            &game->target,
            BASIC_ATTACK_RANGE,
            BASIC_ATTACK_DAMAGE
        );

        game->attackCooldown = BASIC_ATTACK_COOLDOWN;
        game->attackMessageTime = 0.6f;

        if (game->lastAttackResult == WBN_ATTACK_HIT ||
            game->lastAttackResult == WBN_ATTACK_KILLED)
        {
            game->attackEffectTime = ATTACK_EFFECT_DURATION;
        }
    }

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
    AsciiCanvas_DrawCharacter(
        &game->canvas,
        '@',
        (Vector2)
        {
            game->playerPosition.x - 0.5f,
            game->playerPosition.y - 0.5f
        },
        GOLD
    );

    Vector2 targetPosition =
    {
        game->target.position.x,
        game->target.position.y
    };
    Color targetColor = WBNCombatTarget_IsAlive(&game->target) ? MAROON : DARKGRAY;

    if (game->attackEffectTime > 0.0f)
    {
        Vector2 playerScreen = AsciiCanvas_CellToScreen(&game->canvas, game->playerPosition);
        Vector2 targetScreen = AsciiCanvas_CellToScreen(&game->canvas, targetPosition);
        DrawLineEx(playerScreen, targetScreen, 2.0f, GOLD);
        targetColor = YELLOW;
    }

    AsciiCanvas_DrawCharacter(
        &game->canvas,
        WBNCombatTarget_IsAlive(&game->target) ? 'D' : '%',
        (Vector2){ targetPosition.x - 0.5f, targetPosition.y - 0.5f },
        targetColor
    );

    DrawText("WASD move  |  SPACE attack  |  ` console", 16, 16, 18, RAYWHITE);

    if (WBNCombatTarget_IsAlive(&game->target))
    {
        DrawText(
            TextFormat("Target health: %d / %d", game->target.health, game->target.maxHealth),
            16,
            42,
            18,
            LIGHTGRAY
        );
    }
    else
    {
        DrawText("Target defeated", 16, 42, 18, GOLD);
    }

    if (game->attackMessageTime > 0.0f &&
        game->lastAttackResult == WBN_ATTACK_OUT_OF_RANGE)
    {
        DrawText("Out of range", 16, 68, 18, ORANGE);
    }
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
