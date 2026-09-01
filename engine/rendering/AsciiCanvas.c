#include "AsciiCanvas.h"

#include <stdio.h>

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

    canvas->originX = originX;
    canvas->originY = originY;
    canvas->fontSize = fontSize;
    canvas->spacing = spacing;
    canvas->cellWidth = MeasureText("M", fontSize) + spacing;
    canvas->cellHeight = fontSize + spacing;
    canvas->color = color;

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
            BGrid_SetTile(&canvas->grid, x, y, layer, emptyTile);
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

    return BGrid_SetTile(&canvas->grid, x, y, layer, tile);
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

            BTile tile = BTile_Create((int)row[x]);

            if (row[x] != ' ')
                BTile_AddFlag(&tile, BTILE_FLAG_VISIBLE);

            BGrid_SetTile(&canvas->grid, x, y, layer, tile);
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

                if (!BTile_HasFlag(&tile, BTILE_FLAG_VISIBLE))
                    continue;

                AsciiCanvas_DrawCharacter(
                    canvas,
                    tile.id,
                    (Vector2){ (float)x, (float)y },
                    canvas->color
                );
            }
        }
    }
}
