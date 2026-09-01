#ifndef ASCII_CANVAS_H
#define ASCII_CANVAS_H

#include <stdbool.h>
#include <stddef.h>

#include <raylib.h>
#include <BGrid.h>
#include <BTile.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AsciiCanvas
{
    BGrid grid;

    int originX;
    int originY;
    int fontSize;
    int spacing;
    int cellWidth;
    int cellHeight;
    Color color;
} AsciiCanvas;

bool AsciiCanvas_Init(
    AsciiCanvas* canvas,
    size_t width,
    size_t height,
    size_t layers,
    int originX,
    int originY,
    int fontSize,
    int spacing,
    Color color
);

void AsciiCanvas_Destroy(AsciiCanvas* canvas);

bool AsciiCanvas_LoadLayerFromText(
    AsciiCanvas* canvas,
    const char** lines,
    size_t lineCount,
    size_t layer
);

bool AsciiCanvas_LoadLayerFromFile(
    AsciiCanvas* canvas,
    const char* filePath,
    size_t layer
);

bool AsciiCanvas_SetCharacter(
    AsciiCanvas* canvas,
    size_t x,
    size_t y,
    size_t layer,
    int character,
    bool visible
);

bool AsciiCanvas_GetCharacter(
    const AsciiCanvas* canvas,
    size_t x,
    size_t y,
    size_t layer,
    int* outCharacter,
    bool* outVisible
);

void AsciiCanvas_SetOrigin(AsciiCanvas* canvas, int originX, int originY);
Vector2 AsciiCanvas_GetCellSize(const AsciiCanvas* canvas);
Vector2 AsciiCanvas_CellToScreen(const AsciiCanvas* canvas, Vector2 cellPosition);
void AsciiCanvas_DrawCharacter(
    const AsciiCanvas* canvas,
    int character,
    Vector2 cellPosition,
    Color color
);

void AsciiCanvas_Draw(const AsciiCanvas* canvas);


#ifdef __cplusplus
}
#endif

#endif
