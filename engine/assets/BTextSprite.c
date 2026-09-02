#include "BTextSprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

typedef struct BTextSpriteRow
{
    size_t start;
    size_t width;
} BTextSpriteRow;

static bool BTextSprite_Fail(
    BDiagnosticList* diagnostics,
    BDiagnosticCode code,
    const char* message,
    const char* path,
    size_t line,
    size_t column
)
{
    BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, code, message, path);

    if (diagnostics != NULL && diagnostics->count > 0)
    {
        BDiagnostic* diagnostic = &diagnostics->items[diagnostics->count - 1];
        diagnostic->line = line;
        diagnostic->column = column;
    }

    return false;
}

static bool BTextSprite_IsRelativePath(const char* path)
{
    if (path == NULL || path[0] == '\0' || strlen(path) >= BDIAGNOSTIC_PATH_MAX ||
        path[0] == '/' || path[0] == '\\' ||
        (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':'))
    {
        return false;
    }

    const char* segment = path;

    for (const char* cursor = path; ; ++cursor)
    {
        if (*cursor == '\\')
            return false;

        if (*cursor == '/' || *cursor == '\0')
        {
            size_t length = (size_t)(cursor - segment);

            if (length == 0 || (length == 1 && segment[0] == '.') ||
                (length == 2 && segment[0] == '.' && segment[1] == '.'))
            {
                return false;
            }

            if (*cursor == '\0')
                break;

            segment = cursor + 1;
        }
    }

    return true;
}

static bool BTextSprite_PathWithinRoot(const char* root, const char* candidate)
{
    size_t rootLength = strlen(root);

    if (rootLength == 0)
        return false;

#ifdef _WIN32
    if (_strnicmp(root, candidate, rootLength) != 0)
        return false;
#else
    if (strncmp(root, candidate, rootLength) != 0)
        return false;
#endif

    return candidate[rootLength] == '\0' || candidate[rootLength] == '/' || candidate[rootLength] == '\\';
}

static bool BTextSprite_ResolvePath(
    const char* projectRoot,
    const char* relativePath,
    char output[BDIAGNOSTIC_PATH_MAX],
    BDiagnosticList* diagnostics
)
{
    if (projectRoot == NULL || !BTextSprite_IsRelativePath(relativePath))
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Text Sprite path must be project-relative and cannot escape the Project root.", relativePath, 0, 0);

    char combined[BDIAGNOSTIC_PATH_MAX * 2];
    int written = snprintf(combined, sizeof(combined), "%s/%s", projectRoot, relativePath);

    if (written < 0 || (size_t)written >= sizeof(combined))
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Text Sprite path is too long.", relativePath, 0, 0);

#ifdef _WIN32
    char absoluteRoot[BDIAGNOSTIC_PATH_MAX];
    char absoluteCandidate[BDIAGNOSTIC_PATH_MAX];

    if (_fullpath(absoluteRoot, projectRoot, sizeof(absoluteRoot)) == NULL ||
        _fullpath(absoluteCandidate, combined, sizeof(absoluteCandidate)) == NULL ||
        !BTextSprite_PathWithinRoot(absoluteRoot, absoluteCandidate))
    {
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Text Sprite path escapes the Project root.", relativePath, 0, 0);
    }

    HANDLE rootHandle = CreateFileA(
        absoluteRoot,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
    HANDLE fileHandle = CreateFileA(
        absoluteCandidate,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (rootHandle == INVALID_HANDLE_VALUE || fileHandle == INVALID_HANDLE_VALUE)
    {
        if (rootHandle != INVALID_HANDLE_VALUE)
            CloseHandle(rootHandle);
        if (fileHandle != INVALID_HANDLE_VALUE)
            CloseHandle(fileHandle);
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_IO, "Could not open the Text Sprite or Project root.", relativePath, 0, 0);
    }

    char finalRoot[BDIAGNOSTIC_PATH_MAX];
    char finalCandidate[BDIAGNOSTIC_PATH_MAX];
    DWORD rootLength = GetFinalPathNameByHandleA(rootHandle, finalRoot, sizeof(finalRoot), FILE_NAME_NORMALIZED);
    DWORD candidateLength = GetFinalPathNameByHandleA(fileHandle, finalCandidate, sizeof(finalCandidate), FILE_NAME_NORMALIZED);
    CloseHandle(rootHandle);
    CloseHandle(fileHandle);

    if (rootLength == 0 || rootLength >= sizeof(finalRoot) ||
        candidateLength == 0 || candidateLength >= sizeof(finalCandidate) ||
        !BTextSprite_PathWithinRoot(finalRoot, finalCandidate))
    {
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Resolved Text Sprite path escapes the Project root.", relativePath, 0, 0);
    }

    snprintf(output, BDIAGNOSTIC_PATH_MAX, "%s", absoluteCandidate);
#else
    char absoluteRoot[PATH_MAX];
    char absoluteCandidate[PATH_MAX];

    if (realpath(projectRoot, absoluteRoot) == NULL || realpath(combined, absoluteCandidate) == NULL)
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_IO, "Could not resolve the Text Sprite or Project root.", relativePath, 0, 0);

    if (!BTextSprite_PathWithinRoot(absoluteRoot, absoluteCandidate) || strlen(absoluteCandidate) >= BDIAGNOSTIC_PATH_MAX)
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Resolved Text Sprite path escapes the Project root.", relativePath, 0, 0);

    snprintf(output, BDIAGNOSTIC_PATH_MAX, "%s", absoluteCandidate);
#endif

    return true;
}

void BTextSprite_Init(BTextSprite* sprite)
{
    if (sprite != NULL)
        memset(sprite, 0, sizeof(*sprite));
}

void BTextSprite_Destroy(BTextSprite* sprite)
{
    if (sprite == NULL)
        return;

    free(sprite->cells);
    memset(sprite, 0, sizeof(*sprite));
}

void BTextSprite_Swap(BTextSprite* left, BTextSprite* right)
{
    if (left == NULL || right == NULL || left == right)
        return;

    BTextSprite temporary = *left;
    *left = *right;
    *right = temporary;
}

bool BTextSprite_Decode(
    const char* contents,
    size_t length,
    const char* relativePath,
    BTextSprite* destination,
    BDiagnosticList* diagnostics
)
{
    BDiagnosticList_Clear(diagnostics);
    if (contents == NULL || destination == NULL)
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Text Sprite source and destination are required.", relativePath, 0, 0);
    BTextSprite decoded;
    BTextSprite_Init(&decoded);
    BTextSpriteRow rows[BTEXT_SPRITE_HEIGHT_MAX];
    size_t rowCount = 0;
    size_t rowStart = 0;
    size_t rowLength = 0;
    size_t trimmedWidth = 0;
    size_t maximumWidth = 0;
    bool hasGlyph = false;

    for (size_t i = 0; i < length; ++i)
    {
        unsigned char value = (unsigned char)contents[i];

        if (value == '\r')
        {
            if (i + 1 >= length || contents[i + 1] != '\n')
                return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Text Sprite contains a lone carriage return.", relativePath, rowCount + 1, rowLength + 1);

            value = '\n';
            i += 1;
        }

        if (value == '\n')
        {
            if (rowCount >= BTEXT_SPRITE_HEIGHT_MAX)
                return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Text Sprite exceeds the maximum height.", relativePath, rowCount + 1, 1);

            rows[rowCount++] = (BTextSpriteRow){ rowStart, trimmedWidth };
            if (trimmedWidth > maximumWidth)
                maximumWidth = trimmedWidth;
            rowStart = i + 1;
            rowLength = 0;
            trimmedWidth = 0;
            continue;
        }

        if (value < 0x20 || value > 0x7e)
            return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Text Sprite contains a non-printable or non-ASCII byte.", relativePath, rowCount + 1, rowLength + 1);

        rowLength += 1;

        if (value != ' ')
        {
            trimmedWidth = rowLength;
            hasGlyph = true;
        }

        if (trimmedWidth > BTEXT_SPRITE_WIDTH_MAX)
            return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Text Sprite exceeds the maximum width.", relativePath, rowCount + 1, rowLength);
    }

    bool endedWithLineBreak = length > 0 && contents[length - 1] == '\n';

    if (!endedWithLineBreak)
    {
        if (rowCount >= BTEXT_SPRITE_HEIGHT_MAX)
            return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Text Sprite exceeds the maximum height.", relativePath, rowCount + 1, 1);

        rows[rowCount++] = (BTextSpriteRow){ rowStart, trimmedWidth };
        if (trimmedWidth > maximumWidth)
            maximumWidth = trimmedWidth;
    }

    if (!hasGlyph || rowCount == 0 || maximumWidth == 0)
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Text Sprite must contain at least one printable non-space glyph.", relativePath, 0, 0);

    char* cells = (char*)malloc(maximumWidth * rowCount);

    if (cells == NULL)
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY, "Out of memory while decoding the Text Sprite.", relativePath, 0, 0);

    memset(cells, ' ', maximumWidth * rowCount);

    for (size_t row = 0; row < rowCount; ++row)
    {
        if (rows[row].width > 0)
            memcpy(cells + row * maximumWidth, contents + rows[row].start, rows[row].width);
    }

    decoded.width = maximumWidth;
    decoded.height = rowCount;
    decoded.cells = cells;
    BTextSprite_Swap(destination, &decoded);
    BTextSprite_Destroy(&decoded);
    return true;
}

bool BTextSprite_Load(
    const char* projectRoot,
    const char* relativePath,
    BTextSprite* destination,
    BDiagnosticList* diagnostics
)
{
    BDiagnosticList_Clear(diagnostics);

    if (destination == NULL)
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Text Sprite destination is required.", relativePath, 0, 0);

    char path[BDIAGNOSTIC_PATH_MAX];

    if (!BTextSprite_ResolvePath(projectRoot, relativePath, path, diagnostics))
        return false;

    FILE* file = fopen(path, "rb");

    if (file == NULL || fseek(file, 0, SEEK_END) != 0)
    {
        if (file != NULL)
            fclose(file);
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_IO, "Could not open or measure the Text Sprite.", relativePath, 0, 0);
    }

    long measured = ftell(file);

    if (measured < 0 || measured > BTEXT_SPRITE_SOURCE_MAX || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Text Sprite exceeds its source-size limit or could not be measured.", relativePath, 0, 0);
    }

    size_t length = (size_t)measured;
    char* contents = (char*)malloc(length == 0 ? 1 : length);

    if (contents == NULL)
    {
        fclose(file);
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY, "Out of memory while reading the Text Sprite.", relativePath, 0, 0);
    }

    bool read = length == 0 || fread(contents, 1, length, file) == length;
    bool closed = fclose(file) == 0;

    if (!read || !closed)
    {
        free(contents);
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_IO, "Could not read the complete Text Sprite.", relativePath, 0, 0);
    }

    BTextSprite decoded;
    BTextSprite_Init(&decoded);
    bool succeeded = BTextSprite_Decode(contents, length, relativePath, &decoded, diagnostics);
    free(contents);

    if (!succeeded)
        return false;

    BTextSprite_Swap(destination, &decoded);
    BTextSprite_Destroy(&decoded);
    return true;
}

char BTextSprite_Cell(const BTextSprite* sprite, size_t column, size_t row)
{
    if (sprite == NULL || sprite->cells == NULL || column >= sprite->width || row >= sprite->height)
        return ' ';

    return sprite->cells[row * sprite->width + column];
}

void BTextSpriteCache_Init(BTextSpriteCache* cache)
{
    if (cache != NULL)
        memset(cache, 0, sizeof(*cache));
}

void BTextSpriteCache_Destroy(BTextSpriteCache* cache)
{
    if (cache == NULL)
        return;

    for (size_t i = 0; i < cache->count; ++i)
        BTextSprite_Destroy(&cache->entries[i].sprite);

    memset(cache, 0, sizeof(*cache));
}

static BTextSpriteCacheEntry* BTextSpriteCache_FindMutable(
    BTextSpriteCache* cache,
    const char* relativePath
)
{
    if (cache == NULL || relativePath == NULL)
        return NULL;

    for (size_t i = 0; i < cache->count; ++i)
    {
        if (strcmp(cache->entries[i].relativePath, relativePath) == 0)
            return &cache->entries[i];
    }

    return NULL;
}

const BTextSprite* BTextSpriteCache_Find(const BTextSpriteCache* cache, const char* relativePath)
{
    BTextSpriteCacheEntry* entry = BTextSpriteCache_FindMutable((BTextSpriteCache*)cache, relativePath);
    return entry != NULL ? &entry->sprite : NULL;
}

bool BTextSpriteCache_Load(
    BTextSpriteCache* cache,
    const char* projectRoot,
    const char* relativePath,
    const BTextSprite** outSprite,
    BDiagnosticList* diagnostics
)
{
    if (outSprite != NULL)
        *outSprite = NULL;

    if (cache == NULL)
    {
        BDiagnosticList_Clear(diagnostics);
        return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT, "Text Sprite cache is required.", relativePath, 0, 0);
    }

    BTextSprite decoded;
    BTextSprite_Init(&decoded);

    if (!BTextSprite_Load(projectRoot, relativePath, &decoded, diagnostics))
        return false;

    BTextSpriteCacheEntry* entry = BTextSpriteCache_FindMutable(cache, relativePath);

    if (entry == NULL)
    {
        if (cache->count >= BTEXT_SPRITE_CACHE_MAX)
        {
            BTextSprite_Destroy(&decoded);
            return BTextSprite_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA, "Text Sprite cache capacity has been reached.", relativePath, 0, 0);
        }

        entry = &cache->entries[cache->count++];
        snprintf(entry->relativePath, sizeof(entry->relativePath), "%s", relativePath);
    }

    BTextSprite_Swap(&entry->sprite, &decoded);
    BTextSprite_Destroy(&decoded);

    if (outSprite != NULL)
        *outSprite = &entry->sprite;

    return true;
}
