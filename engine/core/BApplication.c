#include "BApplication.h"
#include "BLog.h"
#include "BTime.h"

#include <raylib.h>

bool BApplication_Init(BApplication *app, BEngineConfig config, BApplicationCallbacks callbacks)
{
    if (app == 0)
        return false;

    app->callbacks = callbacks;

    if (!BEngine_Init(&app->engine, config))
        return false;

    if (app->callbacks.onStart != 0) {
        if (!app->callbacks.onStart(app->callbacks.userData, &app->engine)) {
            if (app->callbacks.onShutdown != 0)
                app->callbacks.onShutdown(app->callbacks.userData, &app->engine);

            BEngine_Shutdown(&app->engine);
            return false;
        }
    }

    return true;
}

int BApplication_Run(BApplication *app)
{
    if (app == 0 || !app->engine.isInitialized)
        return 1;

    while (!BEngine_ShouldClose(&app->engine) && !BEngine_IsQuitRequested()) {
        float deltaTime = BTime_GetDeltaTime();
        BTime_Update();
        if (BTime_GetFrameCount() == 60) {
            BLog_InfoF("Frame test reached. Current FPS: %d", BTime_GetFPS());
        }

        if (app->callbacks.onUpdate != 0)
            app->callbacks.onUpdate(app->callbacks.userData, &app->engine, deltaTime);

        BEngine_BeginFrame(&app->engine);

        if (app->callbacks.onRender != 0)
            app->callbacks.onRender(app->callbacks.userData, &app->engine);

        BEngine_EndFrame(&app->engine);
    }

    BApplication_Shutdown(app);

    return 0;
}

void BApplication_Shutdown(BApplication *app)
{
    if (app == 0)
        return;

    if (app->callbacks.onShutdown != 0)
        app->callbacks.onShutdown(app->callbacks.userData, &app->engine);

    BEngine_Shutdown(&app->engine);
}
