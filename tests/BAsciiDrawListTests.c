#include "BAsciiDrawList.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define MAKE_DIRECTORY(path) _mkdir(path)
#define REMOVE_DIRECTORY(path) _rmdir(path)
#define GET_PROCESS_ID() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIRECTORY(path) mkdir(path, 0755)
#define REMOVE_DIRECTORY(path) rmdir(path)
#define GET_PROCESS_ID() getpid()
#endif

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;
    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

static bool WriteText(const char* path, const char* text)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL)
        return false;
    size_t length = strlen(text);
    bool succeeded = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && succeeded;
}

static bool AddRenderableEntity(
    BWorkspaceDocument* document,
    const char* name,
    BTransform2D transform,
    const BAsciiRenderable* renderable,
    size_t* outIndex,
    BDiagnosticList* diagnostics
)
{
    size_t index = 0;
    if (!BWorkspaceDocument_AddEntity(document, name, &index, diagnostics) ||
        !BWorkspaceDocument_AddTransform2D(document, index, transform, true, diagnostics) ||
        !BWorkspaceDocument_AddAsciiRenderable(document, index, renderable, true, diagnostics))
    {
        return false;
    }
    if (outIndex != NULL)
        *outIndex = index;
    return true;
}

int main(void)
{
    int failures = 0;
    char root[256];
    char assets[320];
    char spritePath[384];
    snprintf(root, sizeof(root), "Draw List Test %ld %d", (long)time(NULL), (int)GET_PROCESS_ID());
    snprintf(assets, sizeof(assets), "%s/assets", root);
    snprintf(spritePath, sizeof(spritePath), "%s/ship.txt", assets);
    failures += Check(MAKE_DIRECTORY(root) == 0, "temporary Project root is created");
    failures += Check(MAKE_DIRECTORY(assets) == 0, "temporary asset directory is created");
    failures += Check(WriteText(spritePath, "A \r\nBC\r\n"), "Text Sprite fixture is written");

    BDiagnosticList diagnostics;
    BWorkspaceDocument document;
    BWorkspaceDocument_Init(&document);
    failures += Check(BWorkspaceDocument_CreateDefault(&document, "Main", "Main", &diagnostics), "Workspace is created");

    BAsciiRenderable highGlyph = BAsciiRenderable_DefaultGlyph('@');
    highGlyph.layer = 5;
    highGlyph.foreground = (BAsciiColor){ 1, 2, 3, 4 };
    failures += Check(
        AddRenderableEntity(&document, "High", (BTransform2D){ 10.25f, 20.5f }, &highGlyph, NULL, &diagnostics),
        "higher-layer glyph entity is added"
    );

    BAsciiRenderable sprite = BAsciiRenderable_DefaultGlyph('?');
    sprite.sourceKind = BASCII_SOURCE_TEXT_SPRITE;
    snprintf(sprite.textSpritePath, sizeof(sprite.textSpritePath), "assets/ship.txt");
    sprite.anchor = BASCII_ANCHOR_CENTER;
    sprite.layer = -2;
    sprite.background = (BAsciiColor){ 9, 8, 7, 6 };
    failures += Check(
        AddRenderableEntity(&document, "Sprite", (BTransform2D){ 4.0f, 8.0f }, &sprite, NULL, &diagnostics),
        "centered Text Sprite entity is added"
    );

    BAsciiRenderable sameLayerGlyph = BAsciiRenderable_DefaultGlyph('Z');
    sameLayerGlyph.layer = -2;
    failures += Check(
        AddRenderableEntity(&document, "Same layer", (BTransform2D){ -1.0f, -2.0f }, &sameLayerGlyph, NULL, &diagnostics),
        "same-layer glyph entity is added"
    );

    BAsciiRenderable topLeftSprite = sprite;
    topLeftSprite.anchor = BASCII_ANCHOR_TOP_LEFT;
    topLeftSprite.layer = 0;
    failures += Check(
        AddRenderableEntity(&document, "Top left", (BTransform2D){ 100.0f, 200.0f }, &topLeftSprite, NULL, &diagnostics),
        "top-left Text Sprite entity is added"
    );

    BAsciiRenderable hidden = BAsciiRenderable_DefaultGlyph('H');
    size_t disabledIndex = 0;
    failures += Check(
        AddRenderableEntity(&document, "Disabled", (BTransform2D){ 0.0f, 0.0f }, &hidden, &disabledIndex, &diagnostics),
        "disabled fixture entity is added"
    );
    document.entities[disabledIndex].enabled = false;
    hidden.visible = false;
    failures += Check(
        AddRenderableEntity(&document, "Invisible", (BTransform2D){ 0.0f, 0.0f }, &hidden, NULL, &diagnostics),
        "invisible fixture entity is added"
    );
    failures += Check(BWorkspaceDocument_AddEntity(&document, "Empty", NULL, &diagnostics), "empty entity is valid");

    BTextSpriteCache cache;
    BTextSpriteCache_Init(&cache);
    BAsciiDrawList list;
    BAsciiDrawList_Init(&list);
    failures += Check(BAsciiDrawList_Build(&document, root, &cache, &list, &diagnostics), "draw list builds");
    failures += Check(list.count == 8, "transparent sprite spaces and suppressed entities emit no items");
    failures += Check(
        list.items[0].glyph == 'A' && list.items[1].glyph == 'B' && list.items[2].glyph == 'C' &&
            list.items[3].glyph == 'Z' && list.items[4].glyph == 'A' && list.items[7].glyph == '@',
        "items sort by layer, entity order, and sprite row/column order"
    );
    failures += Check(
        list.items[0].x == 3.5f && list.items[0].y == 7.5f &&
            list.items[1].x == 3.5f && list.items[1].y == 8.5f &&
            list.items[2].x == 4.5f && list.items[2].y == 8.5f,
        "even centered Text Sprite receives fractional anchor placement"
    );
    failures += Check(
        list.items[4].x == 100.0f && list.items[4].y == 200.0f &&
            list.items[7].x == 10.25f && list.items[7].y == 20.5f &&
            list.items[7].foreground.r == 1 && list.items[0].background.r == 9,
        "positions and component colors are copied into host-neutral items"
    );
    failures += Check(
        strcmp(list.items[0].entityId, document.entities[1].id) == 0 &&
            strcmp(list.items[7].entityId, document.entities[0].id) == 0,
        "draw items retain stable source entity IDs"
    );

    char retainedGlyph = list.items[0].glyph;
    size_t retainedCount = list.count;
    failures += Check(WriteText(spritePath, "bad\tasset\n"), "malformed refresh is written");
    failures += Check(!BAsciiDrawList_Build(&document, root, &cache, &list, &diagnostics), "malformed asset rejects interpretation");
    const BDiagnostic* error = BDiagnosticList_FirstError(&diagnostics);
    failures += Check(
        error != NULL && strcmp(error->entityId, document.entities[1].id) == 0 &&
            strcmp(error->componentType, BWORKSPACE_ASCII_RENDERABLE_TYPE) == 0,
        "asset error identifies its entity and component"
    );
    failures += Check(list.count == retainedCount && list.items[0].glyph == retainedGlyph, "failed build preserves prior draw list");
    const BTextSprite* retainedSprite = BTextSpriteCache_Find(&cache, "assets/ship.txt");
    failures += Check(retainedSprite != NULL && retainedSprite->width == 2, "failed refresh preserves cached asset");

    failures += Check(WriteText(spritePath, "A \nBC\n"), "valid LF refresh is restored");
    BWorkspaceComponent* spriteComponent = BWorkspaceEntity_FindComponent(&document.entities[1], BWORKSPACE_ASCII_RENDERABLE_TYPE);
    spriteComponent->data.asciiRenderable.transparentSpaces = false;
    spriteComponent->data.asciiRenderable.anchor = BASCII_ANCHOR_BOTTOM_CENTER;
    failures += Check(BAsciiDrawList_Build(&document, root, &cache, &list, &diagnostics), "opaque-space bottom-center list builds");
    failures += Check(list.count == 9 && list.items[1].glyph == ' ', "nontransparent spaces remain explicit draw items");
    failures += Check(list.items[0].x == 3.5f && list.items[0].y == 7.0f, "bottom-center anchor uses the bottom row as origin");

    BWorkspaceDocument empty;
    BWorkspaceDocument_Init(&empty);
    failures += Check(BWorkspaceDocument_CreateDefault(&empty, "Empty", "Empty", &diagnostics), "empty Workspace is created");
    failures += Check(BAsciiDrawList_Build(&empty, root, &cache, &list, &diagnostics), "empty Workspace builds successfully");
    failures += Check(list.count == 0, "empty Workspace replaces prior output with an empty list");

    BAsciiDrawList_Destroy(&list);
    BAsciiDrawList_Destroy(&list);
    BTextSpriteCache_Destroy(&cache);
    BTextSpriteCache_Destroy(&cache);
    BWorkspaceDocument_Destroy(&empty);
    BWorkspaceDocument_Destroy(&document);
    remove(spritePath);
    failures += Check(REMOVE_DIRECTORY(assets) == 0, "temporary asset directory is removed");
    failures += Check(REMOVE_DIRECTORY(root) == 0, "temporary Project root is removed");

    if (failures == 0)
        printf("BAsciiDrawListTests passed.\n");
    return failures == 0 ? 0 : 1;
}
