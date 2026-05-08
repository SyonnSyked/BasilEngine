#include "BEngine.h"

#include <raylib.h>

BEngineConfig BEngineConfig_Default(void)
{
    BEngineConfig config;

    config.windowWidth = 800;
    config.windowHeight = 450;
    config.windowTitle = "BasilEngine Application";
    config.targetFPS = 60;

    return config;
}

bool BEngine_Init(BEngine* engine, BEngineConfig config)
{
    if (engine == 0)
        return false;

    if (config.windowWidth <= 0)
        config.windowWidth = 800;

    if (config.windowHeight <= 0)
        config.windowHeight = 450;

    if (config.windowTitle == 0)
        config.windowTitle = "BasilEngine Application";

    if (config.targetFPS <= 0)
        config.targetFPS = 60;

    InitWindow(config.windowWidth, config.windowHeight, config.windowTitle);

    if (!IsWindowReady())
    {
        engine->isInitialized = false;
        engine->isRunning = false;
        return false;
    }

    SetTargetFPS(config.targetFPS);

    engine->config = config;
    engine->isInitialized = true;
    engine->isRunning = true;

    return true;
}

void BEngine_BeginFrame(BEngine* engine)
{
    if (engine == 0 || !engine->isInitialized)
        return;

    BeginDrawing();
}

void BEngine_EndFrame(BEngine* engine)
{
    if (engine == 0 || !engine->isInitialized)
        return;

    EndDrawing();
}

bool BEngine_ShouldClose(const BEngine* engine)
{
    if (engine == 0 || !engine->isInitialized)
        return true;

    return WindowShouldClose();
}

void BEngine_Shutdown(BEngine* engine)
{
    if (engine == 0 || !engine->isInitialized)
        return;

    CloseWindow();

    engine->isRunning = false;
    engine->isInitialized = false;
}
