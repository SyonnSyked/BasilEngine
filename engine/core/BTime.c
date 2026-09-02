#include "BTime.h"
#include <raylib.h>

static unsigned long long g_FrameCount = 0;

float BTime_GetDeltaTime() {
    return GetFrameTime();
}

float BTime_GetTime() {
    return (float)GetTime();
}

int BTime_GetFPS() {
    return GetFPS();
}

unsigned long long BTime_GetFrameCount() {
    return g_FrameCount;
}

void BTime_Update() {
    g_FrameCount++;
}
