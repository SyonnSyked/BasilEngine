#include <raylib.h>

#include "AsciiCanvas.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Where Birds Nest - ASCII Canvas");

    const char* cabinArt[] =
    {
        "        *                 *          ",
        "              /\\                    ",
        "             /  \\       *            ",
        "    *       /____\\                   ",
        "           | [] [] |                 ",
        "           |  __   |        *        ",
        "    --- ---| |  |  |--- ---          ",
        "     . . . |_|__|__| . . .           "
    };

    AsciiCanvas canvas;

    if (!AsciiCanvas_Init(&canvas, 40, 12, 1, 80, 80, 18, 1, RAYWHITE))
    {
        CloseWindow();
        return 1;
    }

    AsciiCanvas_LoadLayerFromText(
        &canvas,
        cabinArt,
        sizeof(cabinArt) / sizeof(cabinArt[0]),
        0
    );

    while (!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(BLACK);
        AsciiCanvas_Draw(&canvas);

        EndDrawing();
    }

    AsciiCanvas_Destroy(&canvas);
    CloseWindow();

    return 0;
}
