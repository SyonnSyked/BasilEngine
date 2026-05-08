#ifndef BAPPLICATION_H
#define BAPPLICATION_H

#include <stdbool.h>

#include "BEngine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*BApplicationStartFn)(void* userData, BEngine* engine);
typedef void (*BApplicationUpdateFn)(void* userData, BEngine* engine, float deltaTime);
typedef void (*BApplicationRenderFn)(void* userData, BEngine* engine);
typedef void (*BApplicationShutdownFn)(void* userData, BEngine* engine);

typedef struct BApplicationCallbacks
{
    BApplicationStartFn onStart;
    BApplicationUpdateFn onUpdate;
    BApplicationRenderFn onRender;
    BApplicationShutdownFn onShutdown;
    void* userData;
} BApplicationCallbacks;

typedef struct BApplication
{
    BEngine engine;
    BApplicationCallbacks callbacks;
} BApplication;

bool BApplication_Init(
    BApplication* app,
    BEngineConfig config,
    BApplicationCallbacks callbacks
);

int BApplication_Run(BApplication* app);
void BApplication_Shutdown(BApplication* app);

#ifdef __cplusplus
}
#endif

#endif
