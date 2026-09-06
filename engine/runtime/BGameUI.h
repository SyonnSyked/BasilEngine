#ifndef BASIL_ENGINE_GAME_UI_INTERNAL_H
#define BASIL_ENGINE_GAME_UI_INTERNAL_H

#include "BGame.h"

#define BGAME_UI_COMMAND_CAPACITY 4096

typedef struct BGameUIGlyphCommand {
    int x;
    int y;
    char glyph;
    BGameUIColor foreground;
    BGameUIColor background;
} BGameUIGlyphCommand;

typedef struct BGameUIContext {
    int columns;
    int rows;
    int *selection;
    int selectableCount;
    int previousSelectableCount;
    int mouseX;
    int mouseY;
    bool movePrevious;
    bool moveNext;
    bool confirm;
    bool mousePressed;
    bool consumed;
    bool active;
    bool overflowed;
    size_t commandCount;
    BGameUIGlyphCommand commands[BGAME_UI_COMMAND_CAPACITY];
} BGameUIContext;

void BGameUIContext_Init(BGameUIContext *ui);
void BGameUIContext_BeginFrame(BGameUIContext *ui, int columns, int rows, int mouseX, int mouseY);
void BGameUIContext_Begin(BGameUIContext *ui, int *selection, BGameUIInput input);
void BGameUIContext_Label(BGameUIContext *ui, BGameUIPosition position, const char *text);
BGameUICell BGameUIContext_Box(BGameUIContext *ui, BGameUIRect rect);
bool BGameUIContext_Choice(BGameUIContext *ui, BGameUIPosition position, const char *text);
bool BGameUIContext_End(BGameUIContext *ui);
BGameUICell BGameUI_ResolveAnchor(BGameUIAnchor anchor, int offsetX, int offsetY, int width,
                                  int height, int columns, int rows);
bool BGameUI_HitTest(int x, int y, int width, int height, int pointX, int pointY);

#endif
