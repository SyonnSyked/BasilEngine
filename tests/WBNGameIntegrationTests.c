#include "BGame.h"

#include <stdio.h>
#include <string.h>

typedef struct TestHost {
    uint32_t generation;
    float playerX;
    float playerY;
    bool confirmPressed;
    bool moveDown;
    bool moveRight;
    bool requested;
    int *selection;
    BGameUIInput uiInput;
    int choiceIndex;
    BGameEntity lastSetEntity;
} TestHost;

static BGameEntity Player(const TestHost *test)
{
    return (BGameEntity){test->generation == 1 ? 1 : 101};
}
static size_t EntityCount(void *context) { return ((TestHost *)context)->generation == 1 ? 2 : 1; }
static BGameEntity EntityAt(void *context, size_t index)
{
    TestHost *test = context;
    return index == 0 ? Player(test) : (BGameEntity){2};
}
static const char *EntityName(void *context, BGameEntity entity)
{
    TestHost *test = context;
    if (entity.value == Player(test).value)
        return "Wayfinder";
    return entity.value == 2 && test->generation == 1 ? "Seamus" : NULL;
}
static bool GetPosition(void *context, BGameEntity entity, float *x, float *y)
{
    TestHost *test = context;
    if (entity.value != Player(test).value)
        return false;
    *x = test->playerX;
    *y = test->playerY;
    return true;
}
static bool SetPosition(void *context, BGameEntity entity, float x, float y)
{
    TestHost *test = context;
    if (entity.value != Player(test).value)
        return false;
    test->playerX = x;
    test->playerY = y;
    test->lastSetEntity = entity;
    return true;
}
static bool GetBounds(void *context, BGameEntity entity, BGameAABB *bounds, bool *trigger)
{
    TestHost *test = context;
    if (entity.value == Player(test).value) {
        *bounds = (BGameAABB){test->playerX - .5f, test->playerY - .5f,
                              test->playerX + .5f, test->playerY + .5f};
        if (trigger) *trigger = false;
        return true;
    }
    if (entity.value == 2 && test->generation == 1) {
        *bounds = (BGameAABB){-2, -2, 2, 2};
        if (trigger) *trigger = true;
        return true;
    }
    return false;
}
static size_t Query(void *context, const BGameAABB *area, BGameEntity ignored,
                    BGameCollisionHit *hits, size_t capacity)
{
    TestHost *test = context;
    if (test->generation != 1 || ignored.value == 2 || area->maxX <= -2 || area->minX >= 2 ||
        area->maxY <= -2 || area->minY >= 2)
        return 0;
    if (hits && capacity)
        hits[0] = (BGameCollisionHit){(BGameEntity){2}, (BGameAABB){-2, -2, 2, 2}, true};
    return 1;
}
static bool InputPressed(void *context, const char *action)
{
    TestHost *test = context;
    return strcmp(action, "confirm") == 0 && test->confirmPressed;
}
static bool InputDown(void *context, const char *action)
{
    TestHost *test = context;
    return (strcmp(action, "move_down") == 0 && test->moveDown) ||
           (strcmp(action, "move_right") == 0 && test->moveRight);
}
static uint32_t Generation(void *context) { return ((TestHost *)context)->generation; }
static bool Request(void *context, const char *path)
{
    TestHost *test = context;
    test->requested = strcmp(path, "workspaces/Testing1.basilworkspace") == 0;
    return test->requested;
}
static void Log(void *context, const char *message) { (void)context; (void)message; }
static void UIBegin(void *context, int *selection, BGameUIInput input)
{
    TestHost *test = context;
    test->selection = selection;
    test->uiInput = input;
    test->choiceIndex = 0;
}
static void UILabel(void *context, BGameUIPosition position, const char *text)
{ (void)context; (void)position; (void)text; }
static BGameUICell UIBox(void *context, BGameUIRect rect)
{ (void)context; return (BGameUICell){rect.x, rect.y}; }
static bool UIChoice(void *context, BGameUIPosition position, const char *text)
{
    TestHost *test = context;
    int index = test->choiceIndex++;
    (void)position;
    (void)text;
    return test->uiInput.confirm && test->selection && *test->selection == index;
}
static bool UIEnd(void *context) { (void)context; return false; }
static int Check(bool condition, const char *message)
{
    if (condition) return 0;
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    int failures = 0;
    TestHost test = {.generation = 1};
    BGameHostAPI host = {.context = &test, .log = Log, .entityCount = EntityCount,
        .entityAt = EntityAt, .entityName = EntityName, .getPosition = GetPosition,
        .setPosition = SetPosition, .getColliderBounds = GetBounds, .queryColliders = Query,
        .inputPressed = InputPressed, .inputDown = InputDown, .requestWorkspace = Request,
        .workspaceGeneration = Generation, .uiBegin = UIBegin, .uiLabel = UILabel,
        .uiBox = UIBox, .uiChoice = UIChoice, .uiEnd = UIEnd};
    void *game = NULL;
    failures += Check(BasilGame_Initialize(&host, &game), "game initializes on Main");
    test.confirmPressed = true;
    BasilGame_Update(game, .1f);
    test.moveDown = true;
    BasilGame_Render(game);
    failures += Check(test.playerY == 0.0f, "opening dialogue does not move the player");
    BasilGame_Update(game, .1f);
    failures += Check(test.playerY == 0.0f, "modal dialogue suppresses gameplay movement");
    BasilGame_Render(game);
    failures += Check(test.requested, "Yes requests Testing1 Workspace");
    test.generation = 2;
    test.confirmPressed = false;
    test.moveDown = false;
    test.moveRight = true;
    BasilGame_Update(game, .1f);
    failures += Check(test.lastSetEntity.value == 101 && test.playerX > 0.0f,
                      "generation change reacquires Wayfinder and movement continues");
    BasilGame_Shutdown(game);
    if (failures == 0) printf("WBNGameIntegrationTests passed.\n");
    return failures == 0 ? 0 : 1;
}
