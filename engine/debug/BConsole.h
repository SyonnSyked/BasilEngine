#ifndef BASIL_ENGINE_CONSOLE_H
#define BASIL_ENGINE_CONSOLE_H

#include <stdbool.h>

void BConsole_Init();
void BConsole_Update();
void BConsole_Draw();
void BConsole_Shutdown();

bool BConsole_IsOpen();

#endif
