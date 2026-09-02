#ifndef BASIL_ENGINE_PROJECT_GENERATOR_H
#define BASIL_ENGINE_PROJECT_GENERATOR_H

#include "BProject.h"

#ifdef __cplusplus
extern "C" {
#endif

bool BProjectGenerator_Create(
    const BProject* project,
    const char* parentDirectory,
    BProjectError* error
);

#ifdef __cplusplus
}
#endif

#endif
