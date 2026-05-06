#include "AsciiCanvas.h"

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
    canvas->color = color;

    return true;
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

                char text[2];
                text[0] = (char)tile.id;
                text[1] = '\0';

                int drawX = canvas->originX + (int)x * (canvas->fontSize + canvas->spacing);
                int drawY = canvas->originY + (int)y * (canvas->fontSize + canvas->spacing);

                DrawText(text, drawX, drawY, canvas->fontSize, canvas->color);
            }
        }
    }
}
