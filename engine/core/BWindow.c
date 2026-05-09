#include "BWindow.h"
#include <raylib.h>


bool BWindow_Init(BWindowConfig config) {

    InitWindow(config.width, config.height, config.title);
    SetTargetFPS(config.targetFPS);

    return true;
}

void BWindow_Shutdown() {
    CloseWindow();
}

bool BWindow_ShouldClose() {
    return WindowShouldClose();
}

void BWindwow_BeginFrame() {
    BeginDrawing();
}

void BWindow_EndFrame() {
    EndDrawing();
}
