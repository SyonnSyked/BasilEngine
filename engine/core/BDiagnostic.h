#ifndef BASIL_ENGINE_DIAGNOSTIC_H
#define BASIL_ENGINE_DIAGNOSTIC_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BDIAGNOSTIC_MAX 32
#define BDIAGNOSTIC_MESSAGE_MAX 256
#define BDIAGNOSTIC_PATH_MAX 1024
#define BDIAGNOSTIC_ENTITY_ID_MAX 32
#define BDIAGNOSTIC_COMPONENT_TYPE_MAX 96
#define BDIAGNOSTIC_DETAIL_MAX 512

typedef enum BDiagnosticSeverity
{
    BDIAGNOSTIC_INFO,
    BDIAGNOSTIC_WARNING,
    BDIAGNOSTIC_ERROR
} BDiagnosticSeverity;

typedef enum BDiagnosticCode
{
    BDIAGNOSTIC_NONE,
    BDIAGNOSTIC_INVALID_ARGUMENT,
    BDIAGNOSTIC_IO,
    BDIAGNOSTIC_INVALID_DATA,
    BDIAGNOSTIC_UNSUPPORTED_VERSION,
    BDIAGNOSTIC_OUT_OF_MEMORY
} BDiagnosticCode;

typedef struct BDiagnostic
{
    BDiagnosticSeverity severity;
    BDiagnosticCode code;
    char message[BDIAGNOSTIC_MESSAGE_MAX];
    char path[BDIAGNOSTIC_PATH_MAX];
    size_t line;
    size_t column;
    char entityId[BDIAGNOSTIC_ENTITY_ID_MAX];
    char componentType[BDIAGNOSTIC_COMPONENT_TYPE_MAX];
    char detail[BDIAGNOSTIC_DETAIL_MAX];
} BDiagnostic;

typedef struct BDiagnosticList
{
    size_t count;
    int truncated;
    BDiagnostic items[BDIAGNOSTIC_MAX];
} BDiagnosticList;

static inline void BDiagnosticList_Clear(BDiagnosticList* diagnostics)
{
    if (diagnostics != NULL)
        memset(diagnostics, 0, sizeof(*diagnostics));
}

static inline void BDiagnosticList_Add(
    BDiagnosticList* diagnostics,
    BDiagnosticSeverity severity,
    BDiagnosticCode code,
    const char* message,
    const char* path
)
{
    if (diagnostics == NULL)
        return;

    if (diagnostics->count >= BDIAGNOSTIC_MAX)
    {
        diagnostics->truncated = 1;
        return;
    }

    BDiagnostic* diagnostic = &diagnostics->items[diagnostics->count++];
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->severity = severity;
    diagnostic->code = code;

    if (message != NULL)
    {
        strncpy(diagnostic->message, message, sizeof(diagnostic->message) - 1);
        diagnostic->message[sizeof(diagnostic->message) - 1] = '\0';
    }

    if (path != NULL)
    {
        strncpy(diagnostic->path, path, sizeof(diagnostic->path) - 1);
        diagnostic->path[sizeof(diagnostic->path) - 1] = '\0';
    }
}

static inline const BDiagnostic* BDiagnosticList_FirstError(const BDiagnosticList* diagnostics)
{
    if (diagnostics == NULL)
        return NULL;

    for (size_t i = 0; i < diagnostics->count; ++i)
    {
        if (diagnostics->items[i].severity == BDIAGNOSTIC_ERROR)
            return &diagnostics->items[i];
    }

    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif
