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

void AsciiCanvas_Draw(const AsciiCanvas* canvas);


#ifdef __cplusplus
}
#endif

#endif
