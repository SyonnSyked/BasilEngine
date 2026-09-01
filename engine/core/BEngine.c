#include "BEngine.h"
#include "BLog.h"
#include "../debug/BConsole.h"
#include "../input/BInput.h"

#include <raylib.h>


static bool g_QuitRequested = false;

BEngineConfig BEngineConfig_Default()
{
    BEngineConfig config;

    config.windowConfig.width = 1600;
    config.windowConfig.height = 900;
    config.windowConfig.title = "BasilEngine Application";
    config.windowConfig.targetFPS = 60;

    return config;
}

bool BEngine_Init(BEngine* engine, BEngineConfig config)
{
    if (engine == 0)
        return false;

    g_QuitRequested = false;

    if (!BWindow_Init(config.windowConfig))
        return false;

    if (!IsWindowReady())
    {
        engine->isInitialized = false;
        engine->isRunning = false;
        return false;
    }


    engine->config = config;
    engine->isInitialized = true;
    engine->isRunning = true;


    BLog_Info("Engine_Init() successful...");

    BLog_InfoF("Window size: %d x %d", config.windowConfig.width, config.windowConfig.height);
    BLog_InfoF("Target FPS: %d", config.windowConfig.targetFPS);
    BConsole_Init();
    BInput_Init();
    return true;
}

void BEngine_BeginFrame(BEngine* engine)
{
    if (engine == 0 || !engine->isInitialized)
        return;

    BWindow_BeginFrame();
}

void BEngine_EndFrame(BEngine* engine)
{
    if (engine == 0 || !engine->isInitialized)
        return;

    BWindow_EndFrame();
}

bool BEngine_ShouldClose(const BEngine* engine)
{
    if (engine == 0 || !engine->isInitialized)
        return true;

    return BWindow_ShouldClose();
}

void BEngine_Shutdown(BEngine* engine)
{
    if (engine == 0 || !engine->isInitialized)
        return;


    engine->isRunning = false;
    engine->isInitialized = false;

    BLog_Info("BasilEngine shutting down...");
    BConsole_Shutdown();
    BInput_Shutdown();
    BWindow_Shutdown();
}

void BEngine_RequestQuit()
{
    g_QuitRequested = true;
}

bool BEngine_IsQuitRequested()
{
    return g_QuitRequested;
}
