#ifndef BASIL_ENGINE_PROJECT_H
#define BASIL_ENGINE_PROJECT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BPROJECT_SCHEMA_VERSION 2
#define BPROJECT_LEGACY_SCHEMA_VERSION 1
#define BPROJECT_NAME_MAX 128
#define BPROJECT_IDENTIFIER_MAX 64
#define BPROJECT_PATH_MAX 1024
#define BPROJECT_ERROR_MESSAGE_MAX 256

typedef enum BProjectLanguageMode {
    BPROJECT_LANGUAGE_C,
    BPROJECT_LANGUAGE_CPP,
    BPROJECT_LANGUAGE_MIXED
} BProjectLanguageMode;

typedef enum BProjectErrorCode {
    BPROJECT_ERROR_NONE,
    BPROJECT_ERROR_INVALID_ARGUMENT,
    BPROJECT_ERROR_INVALID_MANIFEST,
    BPROJECT_ERROR_UNSUPPORTED_VERSION,
    BPROJECT_ERROR_IO,
    BPROJECT_ERROR_ALREADY_EXISTS
} BProjectErrorCode;

typedef struct BProjectError {
    BProjectErrorCode code;
    char message[BPROJECT_ERROR_MESSAGE_MAX];
} BProjectError;

typedef struct BProject {
    int schemaVersion;
    char name[BPROJECT_NAME_MAX];
    char identifier[BPROJECT_IDENTIFIER_MAX];
    BProjectLanguageMode languageMode;
    int cStandard;
    int cppStandard;
    char startupWorkspace[BPROJECT_PATH_MAX];
} BProject;

BProject BProject_Default(const char *name, const char *identifier);

bool BProject_Validate(const BProject *project, BProjectError *error);
bool BProject_Load(const char *manifestPath, BProject *outProject, BProjectError *error);
bool BProject_Save(const BProject *project, const char *manifestPath, BProjectError *error);

bool BProject_IsPortableRelativePath(const char *path);

const char *BProject_LanguageModeToString(BProjectLanguageMode mode);
bool BProject_LanguageModeFromString(const char *value, BProjectLanguageMode *outMode);

#ifdef __cplusplus
}
#endif

#endif
