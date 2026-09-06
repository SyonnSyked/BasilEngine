#include "BGame.h"
#include "WBNGameplay.h"

#include <stdio.h>
#include <string.h>

typedef struct GameState {
    const BGameHostAPI *host;
    BGameEntity player;
    BGameEntity seamus;
    uint32_t workspaceGeneration;
    int dialogueSelection;
    int health;
    bool dialogueOpen;
    bool dialogueOpenedThisFrame;
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

static bool ReacquireWorkspaceEntities(GameState *state)
{
    state->player = FindEntityByName(state->host, "Wayfinder");
    state->seamus = FindEntityByName(state->host, "Seamus");
    state->dialogueOpen = false;
    state->dialogueOpenedThisFrame = false;
    state->dialogueSelection = 0;
    return state->player.value != 0;
}

bool BasilGame_Initialize(const BGameHostAPI *host, void **gameState)
{
    static GameState state;
    memset(&state, 0, sizeof(state));
    state.host = host;
    state.health = 8;
    state.workspaceGeneration = BGame_WorkspaceGeneration(host);
    if (!ReacquireWorkspaceEntities(&state)) {
        host->log(host->context, "Could not find Wayfinder entity.");
        return false;
    }
    *gameState = &state;
    host->log(host->context, "Where Birds Nest game initialized.");
    return true;
}

void BasilGame_Update(void *gameState, float deltaTime)
{
    GameState *state = (GameState *)gameState;
    if (state == NULL || state->host == NULL)
        return;
    const BGameHostAPI *host = state->host;
    uint32_t generation = BGame_WorkspaceGeneration(host);
    if (generation != state->workspaceGeneration) {
        state->workspaceGeneration = generation;
        if (!ReacquireWorkspaceEntities(state)) {
            host->log(host->context, "Workspace replacement has no Wayfinder entity.");
            return;
        }
        host->log(host->context, "Workspace generation changed; game entities reacquired.");
    }
    if (state->dialogueOpen)
        return;
    if (state->seamus.value != 0 && host->inputPressed(host->context, "confirm") &&
        WBN_IsTriggerOverlapping(host, state->player, state->seamus)) {
        state->dialogueOpen = true;
        state->dialogueOpenedThisFrame = true;
        state->dialogueSelection = 0;
        return;
    }
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
    const float speed = 8.0f;
    (void)WBN_MoveWithCollision(host, state->player, moveX * speed * deltaTime,
                                moveY * speed * deltaTime);
}

void BasilGame_Render(void *gameState)
{
    GameState *state = (GameState *)gameState;
    if (state == NULL || state->host == NULL)
        return;
    const BGameHostAPI *host = state->host;
    BGameUIInput input = {0};
    if (state->dialogueOpen) {
        input.previous = host->inputPressed(host->context, "move_up");
        input.next = host->inputPressed(host->context, "move_down");
        input.confirm = !state->dialogueOpenedThisFrame &&
                        host->inputPressed(host->context, "confirm");
        input.pointerActivate = host->inputPressed(host->context, "primary_action");
    }
    BGameUI_Begin(host, &state->dialogueSelection, input);
    char health[32];
    snprintf(health, sizeof(health), "HP: %d / 10", state->health);
    BGameUICell hud =
        BGameUI_Box(host, (BGameUIRect){BGAME_UI_TOP_LEFT, 1, 1, 18, 3});
    BGameUI_Label(host, (BGameUIPosition){BGAME_UI_TOP_LEFT, hud.x + 2, hud.y + 1}, health);
    if (state->dialogueOpen) {
        BGameUICell panel =
            BGameUI_Box(host, (BGameUIRect){BGAME_UI_BOTTOM, 0, -1, 34, 8});
        BGameUI_Label(host, (BGameUIPosition){BGAME_UI_TOP_LEFT, panel.x + 2, panel.y + 1},
                      "Seamus");
        BGameUI_Label(host, (BGameUIPosition){BGAME_UI_TOP_LEFT, panel.x + 2, panel.y + 2},
                      "Are you headed north?");
        if (BGameUI_Choice(host,
                           (BGameUIPosition){BGAME_UI_TOP_LEFT, panel.x + 2, panel.y + 4},
                           "Yes")) {
            if (BGame_RequestWorkspace(host, "workspaces/Testing1.basilworkspace"))
                state->dialogueOpen = false;
        }
        if (BGameUI_Choice(host,
                           (BGameUIPosition){BGAME_UI_TOP_LEFT, panel.x + 2, panel.y + 5},
                           "No"))
            state->dialogueOpen = false;
    }
    (void)BGameUI_End(host);
    state->dialogueOpenedThisFrame = false;
}

void BasilGame_Shutdown(void *gameState)
{
    (void)gameState;
}
