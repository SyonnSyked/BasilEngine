#ifndef BASIL_ENGINE_TIME_H
#define BASIL_ENGINE_TIME_H

float BTime_GetDeltaTime(void);

float BTime_GetTime(void);

int BTime_GetFPS(void);

unsigned long long BTime_GetFrameCount(void);

void BTime_Update(void);

#endif
