#ifndef BENGINE_CONFIG_H
#define BENGINE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BEngineConfig
{
    int windowWidth;
    int windowHeight;
    const char* windowTitle;
    int targetFPS;
} BEngineConfig;

BEngineConfig BEngineConfig_Default(void);

#ifdef __cplusplus
}
#endif

#endif
