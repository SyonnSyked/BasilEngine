#include "BGameModule.h"

#include <string.h>
#include <stdio.h>

typedef struct GameState {
    const BGameHostAPI *host;
    BGameEntity player;
    int dialogueSelection;
    int health;
    const char *dialogueResult;
    bool uiConsumed;
} GameState;

static BGameEntity FindEntityByName(const BGameHostAPI *host, const char *name)
{
    const size_t count = host->entityCount(host->context);

    for (size_t i = 0; i < count; ++i) {
        BGameEntity entity = host->entityAt(host->context, i);
        const char *entityName = host->entityName(host->context, entity);

        if (entityName != NULL && strcmp(entityName, name) == 0)
            return entity;
    }

    return (BGameEntity){0};
}

static bool Game_Initialize(const BGameHostAPI *host, void **gameState)
{
    static GameState state;

    state.host = host;
    state.player = FindEntityByName(host, "Wayfinder");
    state.dialogueSelection = 0;
    state.health = 8;
    state.dialogueResult = "Choose a reply.";
    state.uiConsumed = false;

    if (state.player.value == 0) {
        host->log(host->context, "Could not find Wayfinder entity.");
        return false;
    }

    *gameState = &state;

    host->log(host->context, "Where Birds Nest game module initialized.");

    return true;
}

static void Game_Update(void *gameState, float deltaTime)
{
    GameState *state = (GameState *)gameState;

    if (state == NULL || state->host == NULL)
        return;

    const BGameHostAPI *host = state->host;

    if (host->getPosition == NULL || host->setPosition == NULL || host->inputDown == NULL) {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;

    if (!host->getPosition(host->context, state->player, &x, &y)) {
        return;
    }

    const float speed = 8.0f;

    float moveX = 0.0f;
    float moveY = 0.0f;

    if (host->inputDown(host->context, "move_up"))
        moveY -= 1.0f;

    if (host->inputDown(host->context, "move_down"))
        moveY += 1.0f;

    if (host->inputDown(host->context, "move_left"))
        moveX -= 1.0f;

    if (host->inputDown(host->context, "move_right"))
        moveX += 1.0f;

    if (moveX == 0.0f && moveY == 0.0f)
        return;

    x += moveX * speed * deltaTime;
    y += moveY * speed * deltaTime;

    host->setPosition(host->context, state->player, x, y);
}

static void Game_Render(void *gameState)
{
    GameState *state = (GameState *)gameState;
    if (state == NULL || state->host == NULL)
        return;

    char health[32];
    snprintf(health, sizeof(health), "HP: %d / 10", state->health);

    BGameUI_Begin(state->host, &state->dialogueSelection);
    BGameUI_Box(state->host, (BGameUIRect){BGAME_UI_TOP_LEFT, 1, 1, 18, 3});
    BGameUI_Label(state->host, (BGameUIPosition){BGAME_UI_TOP_LEFT, 3, 2}, health);

    BGameUI_Box(state->host, (BGameUIRect){BGAME_UI_BOTTOM, 0, -1, 34, 8});
    BGameUI_Label(state->host, (BGameUIPosition){BGAME_UI_BOTTOM, -14, -7}, "Stranger");
    BGameUI_Label(state->host, (BGameUIPosition){BGAME_UI_BOTTOM, -14, -6},
                  "Are you headed north?");
    if (BGameUI_Choice(state->host, (BGameUIPosition){BGAME_UI_BOTTOM, -14, -4}, "Yes"))
        state->dialogueResult = "Then travel safely.";
    if (BGameUI_Choice(state->host, (BGameUIPosition){BGAME_UI_BOTTOM, -14, -3}, "No"))
        state->dialogueResult = "The road will wait.";
    BGameUI_Label(state->host, (BGameUIPosition){BGAME_UI_BOTTOM, -14, -2},
                  state->dialogueResult);
    state->uiConsumed = BGameUI_End(state->host);
}
static void Game_Shutdown(void *gameState)
{
    (void)gameState;
}

BGAME_MODULE_EXPORT bool BasilGame_Query(uint32_t hostVersion, BGameModule *module)
{
    if (hostVersion != BGAME_API_VERSION || module == 0)
        return false;
    memset(module, 0, sizeof(*module));
    module->version = BGAME_API_VERSION;
    module->structSize = sizeof(*module);
    module->name = "WhereBirdsNest";
    module->onInitialize = Game_Initialize;
    module->onUpdate = Game_Update;
    module->onRender = Game_Render;
    module->onShutdown = Game_Shutdown;
    return true;
}
