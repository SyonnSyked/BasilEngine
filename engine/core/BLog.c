#include "BLog.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define BLOG_MAX_ENTRIES 256

static BLogEntry g_LogEntries[BLOG_MAX_ENTRIES];
static size_t g_LogEntryCount = 0;

static const char *BLog_LevelToString(BLogLevel level)
{
    switch (level) {
        case BLOG_LEVEL_INFO:
            return "INFO";

        case BLOG_LEVEL_WARNING:
            return "WARNING";

        case BLOG_LEVEL_ERROR:
            return "ERROR";

        case BLOG_LEVEL_DEBUG:
            return "DEBUG";

        default:
            return "UNKNOWN";
    }
}

static void BLog_Add(BLogLevel level, const char *message)
{
    if (message == NULL) {
        return;
    }

    if (g_LogEntryCount >= BLOG_MAX_ENTRIES) {
        for (size_t i = 1; i < BLOG_MAX_ENTRIES; i++) {
            g_LogEntries[i - 1] = g_LogEntries[i];
        }

        g_LogEntryCount = BLOG_MAX_ENTRIES - 1;
    }

    BLogEntry *entry = &g_LogEntries[g_LogEntryCount];

    entry->level = level;

    snprintf(entry->message, BLOG_MESSAGE_MAX, "[%s] %s", BLog_LevelToString(level), message);

    g_LogEntryCount++;

    printf("%s\n", entry->message);
    fflush(stdout);
}

static void BLog_AddF(BLogLevel level, const char *format, va_list args)
{
    if (format == NULL) {
        return;
    }

    char buffer[BLOG_MESSAGE_MAX];

    vsnprintf(buffer, sizeof(buffer), format, args);

    BLog_Add(level, buffer);
}

void BLog_Info(const char *message)
{
    BLog_Add(BLOG_LEVEL_INFO, message);
}

void BLog_Warning(const char *message)
{
    BLog_Add(BLOG_LEVEL_WARNING, message);
}

void BLog_Error(const char *message)
{
    BLog_Add(BLOG_LEVEL_ERROR, message);
}

void BLog_Debug(const char *message)
{
    BLog_Add(BLOG_LEVEL_DEBUG, message);
}

void BLog_InfoF(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    BLog_AddF(BLOG_LEVEL_INFO, format, args);
    va_end(args);
}

void BLog_WarningF(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    BLog_AddF(BLOG_LEVEL_WARNING, format, args);
    va_end(args);
}

void BLog_ErrorF(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    BLog_AddF(BLOG_LEVEL_ERROR, format, args);
    va_end(args);
}

void BLog_DebugF(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    BLog_AddF(BLOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

const BLogEntry *BLog_GetEntries(void)
{
    return g_LogEntries;
}

size_t BLog_GetEntryCount(void)
{
    return g_LogEntryCount;
}

void BLog_Clear(void)
{
    g_LogEntryCount = 0;
}
