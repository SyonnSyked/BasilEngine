#ifndef BASIL_ENGINE_GAME_MODULE_H
#define BASIL_ENGINE_GAME_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define BGAME_MODULE_EXPORT __declspec(dllexport)
#else
#define BGAME_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BGAME_API_VERSION 1u

typedef struct BGameEntity {
    uint64_t value;
} BGameEntity;

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
} BGameHostAPI;

typedef struct BGameModule {
    uint32_t version;
    size_t structSize;
    const char *name;
    bool (*onInitialize)(const BGameHostAPI *host, void **gameState);
    void (*onUpdate)(void *gameState, float deltaTime);
    void (*onRender)(void *gameState);
    void (*onShutdown)(void *gameState);
} BGameModule;

typedef bool (*BGameModuleQueryFn)(uint32_t hostVersion, BGameModule *module);

#define BGAME_MODULE_QUERY_NAME "BasilGame_Query"
BGAME_MODULE_EXPORT bool BasilGame_Query(uint32_t hostVersion, BGameModule *module);

#ifdef __cplusplus
}
#endif

#endif
