#include "BAsciiDrawList.h"

#include <stdlib.h>
#include <string.h>

static bool BAsciiDrawList_Fail(BDiagnosticList *diagnostics, BDiagnosticCode code,
                                const char *message, const BWorkspaceEntity *entity)
{
    BDiagnosticList_Add(diagnostics, BDIAGNOSTIC_ERROR, code, message, NULL);

    if (diagnostics != NULL && diagnostics->count > 0 && entity != NULL) {
        BDiagnostic *diagnostic = &diagnostics->items[diagnostics->count - 1];
        strncpy(diagnostic->entityId, entity->id, sizeof(diagnostic->entityId) - 1);
        strncpy(diagnostic->componentType, BWORKSPACE_ASCII_RENDERABLE_TYPE,
                sizeof(diagnostic->componentType) - 1);
    }

    return false;
}

static void BAsciiDrawList_AttachContext(BDiagnosticList *diagnostics,
                                         const BWorkspaceEntity *entity)
{
    if (diagnostics == NULL || entity == NULL)
        return;

    for (size_t i = 0; i < diagnostics->count; ++i) {
        if (diagnostics->items[i].severity == BDIAGNOSTIC_ERROR) {
            strncpy(diagnostics->items[i].entityId, entity->id,
                    sizeof(diagnostics->items[i].entityId) - 1);
            strncpy(diagnostics->items[i].componentType, BWORKSPACE_ASCII_RENDERABLE_TYPE,
                    sizeof(diagnostics->items[i].componentType) - 1);
            return;
        }
    }
}

static bool BAsciiDrawList_Reserve(BAsciiDrawList *list, size_t required,
                                   BDiagnosticList *diagnostics)
{
    if (required > BASCII_DRAW_LIST_ITEM_MAX)
        return BAsciiDrawList_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                   "ASCII draw-list item limit exceeded.", NULL);

    if (required <= list->capacity)
        return true;

    size_t capacity = list->capacity == 0 ? 64 : list->capacity;
    while (capacity < required) {
        if (capacity >= BASCII_DRAW_LIST_ITEM_MAX / 2) {
            capacity = BASCII_DRAW_LIST_ITEM_MAX;
            break;
        }
        capacity *= 2;
    }

    BAsciiDrawItem *items = (BAsciiDrawItem *)realloc(list->items, capacity * sizeof(*items));
    if (items == NULL)
        return BAsciiDrawList_Fail(diagnostics, BDIAGNOSTIC_OUT_OF_MEMORY,
                                   "Could not allocate the ASCII draw list.", NULL);

    list->items = items;
    list->capacity = capacity;
    return true;
}

static bool BAsciiDrawList_Append(BAsciiDrawList *list, char glyph, float x, float y,
                                  const BAsciiRenderable *renderable,
                                  const BWorkspaceEntity *entity, size_t entityOrder,
                                  size_t cellOrder, BDiagnosticList *diagnostics)
{
    if (!BAsciiDrawList_Reserve(list, list->count + 1, diagnostics))
        return false;

    BAsciiDrawItem *item = &list->items[list->count++];
    memset(item, 0, sizeof(*item));
    item->glyph = glyph;
    item->x = x;
    item->y = y;
    item->foreground = renderable->foreground;
    item->background = renderable->background;
    item->layer = renderable->layer;
    item->entityOrder = entityOrder;
    item->cellOrder = cellOrder;
    strncpy(item->entityId, entity->id, sizeof(item->entityId) - 1);
    return true;
}

static void BAsciiDrawList_AnchorOffset(BAsciiAnchor anchor, size_t width, size_t height, float *x,
                                        float *y)
{
    *x = 0.0f;
    *y = 0.0f;

    if (anchor == BASCII_ANCHOR_CENTER || anchor == BASCII_ANCHOR_BOTTOM_CENTER)
        *x = ((float)width - 1.0f) * 0.5f;
    if (anchor == BASCII_ANCHOR_CENTER)
        *y = ((float)height - 1.0f) * 0.5f;
    else if (anchor == BASCII_ANCHOR_BOTTOM_CENTER)
        *y = (float)height - 1.0f;
}

static int BAsciiDrawItem_Compare(const void *leftValue, const void *rightValue)
{
    const BAsciiDrawItem *left = (const BAsciiDrawItem *)leftValue;
    const BAsciiDrawItem *right = (const BAsciiDrawItem *)rightValue;

    if (left->layer != right->layer)
        return left->layer < right->layer ? -1 : 1;
    if (left->entityOrder != right->entityOrder)
        return left->entityOrder < right->entityOrder ? -1 : 1;
    if (left->cellOrder != right->cellOrder)
        return left->cellOrder < right->cellOrder ? -1 : 1;
    return 0;
}

void BAsciiDrawList_Init(BAsciiDrawList *list)
{
    if (list != NULL)
        memset(list, 0, sizeof(*list));
}

void BAsciiDrawList_Destroy(BAsciiDrawList *list)
{
    if (list == NULL)
        return;
    free(list->items);
    memset(list, 0, sizeof(*list));
}

void BAsciiDrawList_Swap(BAsciiDrawList *left, BAsciiDrawList *right)
{
    if (left == NULL || right == NULL || left == right)
        return;
    BAsciiDrawList temporary = *left;
    *left = *right;
    *right = temporary;
}

bool BWorkspaceDocument_ValidateTextSprites(const BWorkspaceDocument *document,
                                            const char *projectRoot, BTextSpriteCache *spriteCache,
                                            BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);
    if (document == NULL || projectRoot == NULL || projectRoot[0] == '\0' || spriteCache == NULL)
        return BAsciiDrawList_Fail(diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
                                   "Workspace, Project root, and Text Sprite cache are required.",
                                   NULL);
    if (!BWorkspaceDocument_Validate(document, diagnostics))
        return false;

    for (size_t entityIndex = 0; entityIndex < document->entityCount; ++entityIndex) {
        const BWorkspaceEntity *entity = &document->entities[entityIndex];
        const BWorkspaceComponent *component =
            BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_ASCII_RENDERABLE_TYPE);
        if (component == NULL ||
            component->data.asciiRenderable.sourceKind != BASCII_SOURCE_TEXT_SPRITE)
            continue;
        const BTextSprite *sprite = NULL;
        if (!BTextSpriteCache_Load(spriteCache, projectRoot,
                                   component->data.asciiRenderable.textSprite.path, &sprite,
                                   diagnostics)) {
            BAsciiDrawList_AttachContext(diagnostics, entity);
            return false;
        }
    }
    return true;
}

bool BAsciiDrawList_Build(const BWorkspaceDocument *document, const char *projectRoot,
                          BTextSpriteCache *spriteCache, BAsciiDrawList *destination,
                          BDiagnosticList *diagnostics)
{
    BDiagnosticList_Clear(diagnostics);
    if (document == NULL || projectRoot == NULL || projectRoot[0] == '\0' || spriteCache == NULL ||
        destination == NULL)
        return BAsciiDrawList_Fail(
            diagnostics, BDIAGNOSTIC_INVALID_ARGUMENT,
            "Workspace, Project root, Text Sprite cache, and destination draw list are required.",
            NULL);

    if (!BWorkspaceDocument_ValidateTextSprites(document, projectRoot, spriteCache, diagnostics))
        return false;

    BAsciiDrawList built;
    BAsciiDrawList_Init(&built);

    for (size_t entityIndex = 0; entityIndex < document->entityCount; ++entityIndex) {
        const BWorkspaceEntity *entity = &document->entities[entityIndex];
        const BWorkspaceComponent *transformComponent =
            BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_TRANSFORM2D_TYPE);
        const BWorkspaceComponent *renderComponent =
            BWorkspaceEntity_FindComponentConst(entity, BWORKSPACE_ASCII_RENDERABLE_TYPE);
        if (!entity->enabled || renderComponent == NULL ||
            !renderComponent->data.asciiRenderable.visible)
            continue;

        if (transformComponent == NULL) {
            BAsciiDrawList_Destroy(&built);
            return BAsciiDrawList_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                       "An enabled ASCII Renderable requires Transform2D.", entity);
        }

        const BTransform2D *transform = &transformComponent->data.transform2d;
        const BAsciiRenderable *renderable = &renderComponent->data.asciiRenderable;
        const BTextSprite *sprite = NULL;
        size_t width = 1;
        size_t height = 1;

        if (renderable->sourceKind == BASCII_SOURCE_TEXT_SPRITE) {
            sprite = BTextSpriteCache_Find(spriteCache, renderable->textSprite.path);
            if (sprite == NULL) {
                BAsciiDrawList_Destroy(&built);
                return BAsciiDrawList_Fail(diagnostics, BDIAGNOSTIC_INVALID_DATA,
                                           "Validated Text Sprite is missing from the cache.",
                                           entity);
            }
            width = sprite->width;
            height = sprite->height;
        }

        float anchorX;
        float anchorY;
        BAsciiDrawList_AnchorOffset(renderable->anchor, width, height, &anchorX, &anchorY);

        for (size_t row = 0; row < height; ++row) {
            for (size_t column = 0; column < width; ++column) {
                char glyph =
                    sprite == NULL ? renderable->glyph : BTextSprite_Cell(sprite, column, row);
                if (glyph == ' ' && renderable->transparentSpaces)
                    continue;
                if (!BAsciiDrawList_Append(&built, glyph, transform->x - anchorX + (float)column,
                                           transform->y - anchorY + (float)row, renderable, entity,
                                           entityIndex, row * width + column, diagnostics)) {
                    BAsciiDrawList_Destroy(&built);
                    return false;
                }
            }
        }
    }

    if (built.count > 1)
        qsort(built.items, built.count, sizeof(*built.items), BAsciiDrawItem_Compare);

    BAsciiDrawList_Swap(destination, &built);
    BAsciiDrawList_Destroy(&built);
    return true;
}
