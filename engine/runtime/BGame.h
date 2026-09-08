#ifndef BASIL_ENGINE_GAME_H
#define BASIL_ENGINE_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BGameEntity {
    uint64_t value;
} BGameEntity;

typedef struct BGameAABB {
    float minX;
    float minY;
    float maxX;
    float maxY;
} BGameAABB;

typedef struct BGameCollisionHit {
    BGameEntity entity;
    BGameAABB bounds;
    bool trigger;
} BGameCollisionHit;

typedef struct BGameUIColor {
    unsigned char r, g, b, a;
} BGameUIColor;
typedef struct BGameUICell {
    int x, y;
} BGameUICell;
typedef enum BGameUIAnchor {
    BGAME_UI_TOP_LEFT,
    BGAME_UI_TOP,
    BGAME_UI_TOP_RIGHT,
    BGAME_UI_LEFT,
    BGAME_UI_CENTER,
    BGAME_UI_RIGHT,
    BGAME_UI_BOTTOM_LEFT,
    BGAME_UI_BOTTOM,
    BGAME_UI_BOTTOM_RIGHT
} BGameUIAnchor;
typedef struct BGameUIPosition {
    BGameUIAnchor anchor;
    int x, y;
} BGameUIPosition;
typedef struct BGameUIRect {
    BGameUIAnchor anchor;
    int x, y, width, height;
} BGameUIRect;
typedef struct BGameUIInput {
    bool previous;
    bool next;
    bool confirm;
    bool pointerActivate;
} BGameUIInput;

typedef struct BGameHostAPI {
    uint32_t version;
    size_t structSize;
    void *context;
    void (*log)(void *context, const char *message);
    const char *(*projectRoot)(void *context);
    size_t (*entityCount)(void *context);
    BGameEntity (*entityAt)(void *context, size_t index);
    const char *(*entityId)(void *context, BGameEntity entity);
    const char *(*entityName)(void *context, BGameEntity entity);
    bool (*getPosition)(void *context, BGameEntity entity, float *x, float *y);
    bool (*setPosition)(void *context, BGameEntity entity, float x, float y);
    bool (*getColliderBounds)(void *context, BGameEntity entity, BGameAABB *bounds, bool *trigger);
    size_t (*queryColliders)(void *context, const BGameAABB *area, BGameEntity ignoreEntity,
                             BGameCollisionHit *hits, size_t hitCapacity);
    bool (*moveWithCollision)(void *context, BGameEntity entity, float deltaX, float deltaY);
    bool (*isTriggerOverlapping)(void *context, BGameEntity entity, BGameEntity triggerEntity);
    const char *(*componentJson)(void *context, BGameEntity entity, const char *type);
    bool (*inputPressed)(void *context, const char *action);
    bool (*inputDown)(void *context, const char *action);
    bool (*inputReleased)(void *context, const char *action);
    bool (*inputRebindKeyboard)(void *context, const char *action, int key);
    bool (*inputRebindMouse)(void *context, const char *action, int button);
    bool (*inputHasAction)(void *context, const char *action);
    int (*inputBindingCode)(void *context, const char *action);
    int (*inputBindingDevice)(void *context, const char *action);
    bool (*requestWorkspace)(void *context, const char *workspacePath);
    uint32_t (*workspaceGeneration)(void *context);
    void (*uiBegin)(void *context, int *selection, BGameUIInput input);
    void (*uiLabel)(void *context, BGameUIPosition position, const char *text);
    BGameUICell (*uiBox)(void *context, BGameUIRect rect);
    bool (*uiChoice)(void *context, BGameUIPosition position, const char *text);
    bool (*uiEnd)(void *context);
} BGameHostAPI;

static inline void BGame_Log(const BGameHostAPI *host, const char *message)
{
    if (host && host->log)
        host->log(host->context, message);
}
static inline const char *BGame_ProjectRoot(const BGameHostAPI *host)
{
    return host && host->projectRoot ? host->projectRoot(host->context) : NULL;
}
static inline size_t BGame_EntityCount(const BGameHostAPI *host)
{
    return host && host->entityCount ? host->entityCount(host->context) : 0;
}
static inline BGameEntity BGame_EntityAt(const BGameHostAPI *host, size_t index)
{
    return host && host->entityAt ? host->entityAt(host->context, index) : (BGameEntity){0};
}
static inline const char *BGame_EntityId(const BGameHostAPI *host, BGameEntity entity)
{
    return host && host->entityId ? host->entityId(host->context, entity) : NULL;
}
static inline const char *BGame_EntityName(const BGameHostAPI *host, BGameEntity entity)
{
    return host && host->entityName ? host->entityName(host->context, entity) : NULL;
}
static inline bool BGame_GetPosition(const BGameHostAPI *host, BGameEntity entity, float *x,
                                     float *y)
{
    return host && host->getPosition && host->getPosition(host->context, entity, x, y);
}
static inline bool BGame_SetPosition(const BGameHostAPI *host, BGameEntity entity, float x, float y)
{
    return host && host->setPosition && host->setPosition(host->context, entity, x, y);
}
static inline bool BGame_GetColliderBounds(const BGameHostAPI *host, BGameEntity entity,
                                           BGameAABB *bounds, bool *trigger)
{
    return host && host->getColliderBounds &&
           host->getColliderBounds(host->context, entity, bounds, trigger);
}
static inline size_t BGame_QueryColliders(const BGameHostAPI *host, const BGameAABB *area,
                                          BGameEntity ignored, BGameCollisionHit *hits,
                                          size_t capacity)
{
    return host && host->queryColliders
               ? host->queryColliders(host->context, area, ignored, hits, capacity)
               : 0;
}
static inline bool BGame_MoveWithCollision(const BGameHostAPI *host, BGameEntity entity,
                                           float deltaX, float deltaY)
{
    return host && host->moveWithCollision &&
           host->moveWithCollision(host->context, entity, deltaX, deltaY);
}
static inline bool BGame_IsTriggerOverlapping(const BGameHostAPI *host, BGameEntity entity,
                                              BGameEntity triggerEntity)
{
    return host && host->isTriggerOverlapping &&
           host->isTriggerOverlapping(host->context, entity, triggerEntity);
}
static inline const char *BGame_ComponentJson(const BGameHostAPI *host, BGameEntity entity,
                                              const char *type)
{
    return host && host->componentJson ? host->componentJson(host->context, entity, type) : NULL;
}
static inline bool BGame_InputPressed(const BGameHostAPI *host, const char *action)
{
    return host && host->inputPressed && host->inputPressed(host->context, action);
}
static inline bool BGame_InputDown(const BGameHostAPI *host, const char *action)
{
    return host && host->inputDown && host->inputDown(host->context, action);
}
static inline bool BGame_InputReleased(const BGameHostAPI *host, const char *action)
{
    return host && host->inputReleased && host->inputReleased(host->context, action);
}
static inline bool BGame_InputRebindKeyboard(const BGameHostAPI *host, const char *action, int key)
{
    return host && host->inputRebindKeyboard &&
           host->inputRebindKeyboard(host->context, action, key);
}
static inline bool BGame_InputRebindMouse(const BGameHostAPI *host, const char *action, int button)
{
    return host && host->inputRebindMouse && host->inputRebindMouse(host->context, action, button);
}
static inline bool BGame_InputHasAction(const BGameHostAPI *host, const char *action)
{
    return host && host->inputHasAction && host->inputHasAction(host->context, action);
}
static inline int BGame_InputBindingCode(const BGameHostAPI *host, const char *action)
{
    return host && host->inputBindingCode ? host->inputBindingCode(host->context, action) : -1;
}
static inline int BGame_InputBindingDevice(const BGameHostAPI *host, const char *action)
{
    return host && host->inputBindingDevice ? host->inputBindingDevice(host->context, action) : -1;
}

static inline bool BGame_RequestWorkspace(const BGameHostAPI *host, const char *workspacePath)
{
    return host != NULL && host->requestWorkspace != NULL &&
           host->requestWorkspace(host->context, workspacePath);
}

static inline uint32_t BGame_WorkspaceGeneration(const BGameHostAPI *host)
{
    return host != NULL && host->workspaceGeneration != NULL
               ? host->workspaceGeneration(host->context)
               : 0;
}

static inline void BGameUI_Begin(const BGameHostAPI *host, int *selection, BGameUIInput input)
{
    if (host && host->uiBegin)
        host->uiBegin(host->context, selection, input);
}
static inline void BGameUI_Label(const BGameHostAPI *host, BGameUIPosition position,
                                 const char *text)
{
    if (host && host->uiLabel)
        host->uiLabel(host->context, position, text);
}
static inline BGameUICell BGameUI_Box(const BGameHostAPI *host, BGameUIRect rect)
{
    return host && host->uiBox ? host->uiBox(host->context, rect) : (BGameUICell){0, 0};
}
static inline bool BGameUI_Choice(const BGameHostAPI *host, BGameUIPosition position,
                                  const char *text)
{
    return host && host->uiChoice && host->uiChoice(host->context, position, text);
}
static inline bool BGameUI_End(const BGameHostAPI *host)
{
    return host && host->uiEnd && host->uiEnd(host->context);
}

bool BasilGame_Initialize(const BGameHostAPI *host, void **gameState);
void BasilGame_Update(void *gameState, float deltaTime);
void BasilGame_Render(void *gameState);
void BasilGame_Shutdown(void *gameState);

#ifdef __cplusplus
}
#endif

#endif
