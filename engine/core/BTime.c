#include "BTime.h"
#include <raylib.h>

static unsigned long long g_FrameCount = 0;

float BTime_GetDeltaTime(void) {
    return GetFrameTime();
}

float BTime_GetTime(void) {
    return (float)GetTime();
}

int BTime_GetFPS(void) {
    return GetFPS();
}

unsigned long long BTime_GetFrameCount(void) {
    return g_FrameCount;
}

void BTime_Update(void) {
    g_FrameCount++;
}
