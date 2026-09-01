#ifndef BASIL_ENGINE_WINDOW_H
#define BASIL_ENGINE_WINDOW_H

#include <stdbool.h>



typedef struct BWindowConfig {
    int width;
    int height;
    const char* title;
    int targetFPS;

} BWindowConfig;

bool BWindow_Init(BWindowConfig config);
void BWindow_Shutdown(void);

bool BWindow_ShouldClose(void);

void BWindow_BeginFrame(void);
void BWindow_EndFrame(void);

#endif
