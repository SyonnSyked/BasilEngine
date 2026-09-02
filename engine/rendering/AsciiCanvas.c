#include "AsciiCanvas.h"

#include <stdio.h>
#include <stdlib.h>

static bool AsciiCanvas_GetStyleIndex(
    const AsciiCanvas* canvas,
    size_t x,
    size_t y,
    size_t layer,
    size_t* outIndex
)
{
    if (canvas == 0 || outIndex == 0)
        return false;

    size_t width = BGrid_GetWidth(&canvas->grid);
    size_t height = BGrid_GetHeight(&canvas->grid);
    size_t layers = BGrid_GetLayers(&canvas->grid);

    if (x >= width || y >= height || layer >= layers)
        return false;

    *outIndex = x + width * (y + height * layer);
    return true;
}

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
)
{
    if (canvas == 0)
        return false;

    if (!BGrid_Init(&canvas->grid, width, height, layers))
        return false;

    size_t cellCount = BGrid_GetTileCount(&canvas->grid);
    canvas->styles = (AsciiCellStyle*)calloc(cellCount, sizeof(AsciiCellStyle));

    if (canvas->styles == 0)
    {
        BGrid_Destroy(&canvas->grid);
        return false;
    }

    canvas->originX = originX;
    canvas->originY = originY;
    canvas->fontSize = fontSize;
    canvas->spacing = spacing;
    canvas->cellWidth = fontSize / 2 + spacing;
    canvas->cellHeight = fontSize + spacing;
    canvas->color = color;

    for (size_t i = 0; i < cellCount; ++i)
    {
        canvas->styles[i].foreground = color;
        canvas->styles[i].background = BLANK;
    }

    return true;
}

bool AsciiCanvas_LoadLayerFromFile(
    AsciiCanvas* canvas,
    const char* filePath,
    size_t layer
)
{
    if (canvas == 0 || filePath == 0)
        return false;

    if (layer >= BGrid_GetLayers(&canvas->grid))
        return false;

    FILE* file = fopen(filePath, "r");

    if (file == 0)
        return false;

    BTile emptyTile = BTile_Empty();
    size_t width = BGrid_GetWidth(&canvas->grid);
    size_t height = BGrid_GetHeight(&canvas->grid);

    for (size_t y = 0; y < height; ++y)
    {
        for (size_t x = 0; x < width; ++x)
        {
            BGrid_SetTile(&canvas->grid, x, y, layer, emptyTile);
            AsciiCanvas_SetCellColors(canvas, x, y, layer, canvas->color, BLANK);
        }
    }

    size_t x = 0;
    size_t y = 0;
    int character = fgetc(file);

    while (character != EOF && y < height)
    {
        if (character == '\n')
        {
            x = 0;
            ++y;
        }
        else if (character != '\r')
        {
            if (x < width)
            {
                bool visible = character != ' ';
                AsciiCanvas_SetCharacter(canvas, x, y, layer, character, visible);
            }

            ++x;
        }

        character = fgetc(file);
    }

    bool succeeded = ferror(file) == 0;
    fclose(file);
    return succeeded;
}

bool AsciiCanvas_SetCharacter(
    AsciiCanvas* canvas,
    size_t x,
    size_t y,
    size_t layer,
    int character,
    bool visible
)
{
    if (canvas == 0)
        return false;

    BTile tile = BTile_Create(character);

    if (visible)
        BTile_AddFlag(&tile, BTILE_FLAG_VISIBLE);

    if (!BGrid_SetTile(&canvas->grid, x, y, layer, tile))
        return false;

    return AsciiCanvas_SetCellColors(canvas, x, y, layer, canvas->color, BLANK);
}

bool AsciiCanvas_GetCharacter(
    const AsciiCanvas* canvas,
    size_t x,
    size_t y,
    size_t layer,
    int* outCharacter,
    bool* outVisible
)
{
    if (canvas == 0 || outCharacter == 0)
        return false;

    BTile tile;

    if (!BGrid_GetTile(&canvas->grid, x, y, layer, &tile))
        return false;

    *outCharacter = tile.id;

    if (outVisible != 0)
        *outVisible = BTile_HasFlag(&tile, BTILE_FLAG_VISIBLE);

    return true;
}

bool AsciiCanvas_SetCellColors(
    AsciiCanvas* canvas,
    size_t x,
    size_t y,
    size_t layer,
    Color foreground,
    Color background
)
{
    if (canvas == 0 || canvas->styles == 0)
        return false;

    size_t index = 0;

    if (!AsciiCanvas_GetStyleIndex(canvas, x, y, layer, &index))
        return false;

    canvas->styles[index].foreground = foreground;
    canvas->styles[index].background = background;
    return true;
}

bool AsciiCanvas_GetCellColors(
    const AsciiCanvas* canvas,
    size_t x,
    size_t y,
    size_t layer,
    Color* outForeground,
    Color* outBackground
)
{
    if (canvas == 0 || canvas->styles == 0 || outForeground == 0 || outBackground == 0)
        return false;

    size_t index = 0;

    if (!AsciiCanvas_GetStyleIndex(canvas, x, y, layer, &index))
        return false;

    *outForeground = canvas->styles[index].foreground;
    *outBackground = canvas->styles[index].background;
    return true;
}

void AsciiCanvas_SetOrigin(AsciiCanvas* canvas, int originX, int originY)
{
    if (canvas == 0)
        return;

    canvas->originX = originX;
    canvas->originY = originY;
}

Vector2 AsciiCanvas_CellToScreen(const AsciiCanvas* canvas, Vector2 cellPosition)
{
    if (canvas == 0)
        return (Vector2){ 0.0f, 0.0f };

    return (Vector2)
    {
        (float)canvas->originX + cellPosition.x * (float)canvas->cellWidth,
        (float)canvas->originY + cellPosition.y * (float)canvas->cellHeight
    };
}

Vector2 AsciiCanvas_GetCellSize(const AsciiCanvas* canvas)
{
    if (canvas == 0)
        return (Vector2){ 0.0f, 0.0f };

    return (Vector2){ (float)canvas->cellWidth, (float)canvas->cellHeight };
}

void AsciiCanvas_DrawCharacter(
    const AsciiCanvas* canvas,
    int character,
    Vector2 cellPosition,
    Color color
)
{
    if (canvas == 0)
        return;

    char text[2] = { (char)character, '\0' };
    Vector2 screenPosition = AsciiCanvas_CellToScreen(canvas, cellPosition);
    DrawText(text, (int)screenPosition.x, (int)screenPosition.y, canvas->fontSize, color);
}

void AsciiCanvas_Destroy(AsciiCanvas* canvas)
{
    if (canvas == 0)
        return;

    free(canvas->styles);
    canvas->styles = 0;
    BGrid_Destroy(&canvas->grid);
}

bool AsciiCanvas_LoadLayerFromText(
    AsciiCanvas* canvas,
    const char** lines,
    size_t lineCount,
    size_t layer
)
{
    if (canvas == 0 || lines == 0)
        return false;

    if (layer >= BGrid_GetLayers(&canvas->grid))
        return false;

    for (size_t y = 0; y < lineCount; ++y)
    {
        const char* row = lines[y];

        if (row == 0)
            continue;

        for (size_t x = 0; row[x] != '\0'; ++x)
        {
            if (x >= BGrid_GetWidth(&canvas->grid))
                break;

            if (y >= BGrid_GetHeight(&canvas->grid))
                break;

            AsciiCanvas_SetCharacter(
                canvas,
                x,
                y,
                layer,
                (int)row[x],
                row[x] != ' '
            );
        }
    }

    return true;
}

void AsciiCanvas_Draw(const AsciiCanvas* canvas)
{
    if (canvas == 0)
        return;

    size_t width = BGrid_GetWidth(&canvas->grid);
    size_t height = BGrid_GetHeight(&canvas->grid);
    size_t layers = BGrid_GetLayers(&canvas->grid);

    for (size_t layer = 0; layer < layers; ++layer)
    {
        for (size_t y = 0; y < height; ++y)
        {
            for (size_t x = 0; x < width; ++x)
            {
                BTile tile;

                if (!BGrid_GetTile(&canvas->grid, x, y, layer, &tile))
                    continue;

                Color foreground = canvas->color;
                Color background = BLANK;
                AsciiCanvas_GetCellColors(
                    canvas,
                    x,
                    y,
                    layer,
                    &foreground,
                    &background
                );

                Vector2 screenPosition = AsciiCanvas_CellToScreen(
                    canvas,
                    (Vector2){ (float)x, (float)y }
                );

                if (background.a > 0)
                {
                    DrawRectangle(
                        (int)screenPosition.x,
                        (int)screenPosition.y,
                        canvas->cellWidth,
                        canvas->cellHeight,
                        background
                    );
                }

                if (!BTile_HasFlag(&tile, BTILE_FLAG_VISIBLE))
                    continue;

                AsciiCanvas_DrawCharacter(
                    canvas,
                    tile.id,
                    (Vector2){ (float)x, (float)y },
                    foreground
                );
            }
        }
    }
}
