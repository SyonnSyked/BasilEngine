#ifndef BASIL_ENGINE_GAME_MODULE_H
#define BASIL_ENGINE_GAME_MODULE_H

#include "BGame.h"

#ifdef _WIN32
#define BGAME_MODULE_EXPORT __declspec(dllexport)
#else
#define BGAME_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BGAME_API_VERSION 1u

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
