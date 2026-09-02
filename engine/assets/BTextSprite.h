#ifndef BASIL_ENGINE_TEXT_SPRITE_H
#define BASIL_ENGINE_TEXT_SPRITE_H

#include "BDiagnostic.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BTEXT_SPRITE_SOURCE_MAX (128 * 1024)
#define BTEXT_SPRITE_WIDTH_MAX 256
#define BTEXT_SPRITE_HEIGHT_MAX 256
#define BTEXT_SPRITE_CACHE_MAX 256

typedef struct BTextSprite
{
    size_t width;
    size_t height;
    char* cells;
} BTextSprite;

typedef struct BTextSpriteCacheEntry
{
    char relativePath[BDIAGNOSTIC_PATH_MAX];
    BTextSprite sprite;
} BTextSpriteCacheEntry;

typedef struct BTextSpriteCache
{
    size_t count;
    BTextSpriteCacheEntry entries[BTEXT_SPRITE_CACHE_MAX];
} BTextSpriteCache;

void BTextSprite_Init(BTextSprite* sprite);
void BTextSprite_Destroy(BTextSprite* sprite);
void BTextSprite_Swap(BTextSprite* left, BTextSprite* right);

bool BTextSprite_Load(
    const char* projectRoot,
    const char* relativePath,
    BTextSprite* destination,
    BDiagnosticList* diagnostics
);
bool BTextSprite_Decode(
    const char* contents,
    size_t length,
    const char* relativePath,
    BTextSprite* destination,
    BDiagnosticList* diagnostics
);
char BTextSprite_Cell(const BTextSprite* sprite, size_t column, size_t row);

void BTextSpriteCache_Init(BTextSpriteCache* cache);
void BTextSpriteCache_Destroy(BTextSpriteCache* cache);
const BTextSprite* BTextSpriteCache_Find(
    const BTextSpriteCache* cache,
    const char* relativePath
);
bool BTextSpriteCache_Load(
    BTextSpriteCache* cache,
    const char* projectRoot,
    const char* relativePath,
    const BTextSprite** outSprite,
    BDiagnosticList* diagnostics
);

#ifdef __cplusplus
}
#endif

#endif
