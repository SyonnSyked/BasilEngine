#ifndef BASIL_ENGINE_LOG_H
#define BASIL_ENGINE_LOG_H

#include <stddef.h>

#define BLOG_MESSAGE_MAX 256

typedef enum BLogLevel
{
    BLOG_LEVEL_INFO,
    BLOG_LEVEL_WARNING,
    BLOG_LEVEL_ERROR,
    BLOG_LEVEL_DEBUG
} BLogLevel;

typedef struct BLogEntry
{
    BLogLevel level;
    char message[BLOG_MESSAGE_MAX];
} BLogEntry;

void BLog_Info(const char* message);
void BLog_Warning(const char* message);
void BLog_Error(const char* message);
void BLog_Debug(const char* message);

void BLog_InfoF(const char* message, ...);
void BLog_WarningF(const char* message, ...);
void BLog_ErrorF(const char* message, ...);
void BLog_DebugF(const char* message, ...);

const BLogEntry* BLog_GetEntries();
size_t BLog_GetEntryCount();

void BLog_Clear();

#endif
