#ifndef BASIL_ENGINE_ASCII_DRAW_LIST_H
#define BASIL_ENGINE_ASCII_DRAW_LIST_H

#include "BTextSprite.h"
#include "BWorkspace.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BASCII_DRAW_LIST_ITEM_MAX (256 * 1024)

typedef struct BAsciiDrawItem
{
    char glyph;
    float x;
    float y;
    BAsciiColor foreground;
    BAsciiColor background;
    short layer;
    char entityId[BWORKSPACE_ENTITY_ID_MAX];
    size_t entityOrder;
    size_t cellOrder;
} BAsciiDrawItem;

typedef struct BAsciiDrawList
{
    size_t count;
    size_t capacity;
    BAsciiDrawItem* items;
} BAsciiDrawList;

void BAsciiDrawList_Init(BAsciiDrawList* list);
void BAsciiDrawList_Destroy(BAsciiDrawList* list);
void BAsciiDrawList_Swap(BAsciiDrawList* left, BAsciiDrawList* right);

/*
 * Builds a complete host-neutral snapshot. On failure, destination is unchanged.
 * The cache may retain successfully decoded assets for later build attempts.
 */
bool BAsciiDrawList_Build(
    const BWorkspaceDocument* document,
    const char* projectRoot,
    BTextSpriteCache* spriteCache,
    BAsciiDrawList* destination,
    BDiagnosticList* diagnostics
);

#ifdef __cplusplus
}
#endif

#endif
