#include "BWindow.h"
#include <raylib.h>


bool BWindow_Init(BWindowConfig config) {

    InitWindow(config.width, config.height, config.title);
    SetTargetFPS(config.targetFPS);

    return true;
}

void BWindow_Shutdown(void) {
    CloseWindow();
}

bool BWindow_ShouldClose(void) {
    return WindowShouldClose();
}

void BWindow_BeginFrame(void) {
    BeginDrawing();
}

void BWindow_EndFrame(void) {
    EndDrawing();
}
