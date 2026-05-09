#ifndef BENGINE_H
#define BENGINE_H

#include <stdbool.h>

#include "BEngineConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BEngine
{
    BEngineConfig config;
    bool isRunning;
    bool isInitialized;
} BEngine;

bool BEngine_Init(BEngine* engine, BEngineConfig config);
void BEngine_BeginFrame(BEngine* engine);
void BEngine_EndFrame(BEngine* engine);
bool BEngine_ShouldClose(const BEngine* engine);
void BEngine_Shutdown(BEngine* engine);

void BEngine_RequestQuit();
bool BEngine_IsQuitRequested();

#ifdef __cplusplus
}
#endif

#endif
