#ifndef BASIL_ENGINE_PROJECT_GENERATOR_H
#define BASIL_ENGINE_PROJECT_GENERATOR_H

#include "BProject.h"

bool BProjectGenerator_Create(
    const BProject* project,
    const char* parentDirectory,
    BProjectError* error
);

#endif
