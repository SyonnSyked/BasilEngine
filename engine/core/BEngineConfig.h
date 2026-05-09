#ifndef BENGINE_CONFIG_H
#define BENGINE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BWindow.h"

typedef struct BEngineConfig
{
    BWindowConfig windowConfig;
} BEngineConfig;

BEngineConfig BEngineConfig_Default(void);

#ifdef __cplusplus
}
#endif

#endif
