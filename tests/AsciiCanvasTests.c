#include <stdio.h>

#include "AsciiCanvas.h"

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

int main(void)
{
    int failures = 0;
    AsciiCanvas canvas = { 0 };

    failures += Check(BGrid_Init(&canvas.grid, 48, 24, 1), "test grid initializes");

    if (failures == 0)
    {
        failures += Check(
            AsciiCanvas_LoadLayerFromFile(
                &canvas,
                "assets/wherebirdsnest/arena.txt",
                0),
            "arena text asset loads"
        );

        int character = 0;
        bool visible = false;

        failures += Check(
            AsciiCanvas_GetCharacter(&canvas, 0, 0, 0, &character, &visible),
            "first arena cell can be read"
        );
        failures += Check(character == '#' && visible, "arena boundary glyph is retained");

        failures += Check(
            AsciiCanvas_GetCharacter(&canvas, 1, 1, 0, &character, &visible),
            "arena floor cell can be read"
        );
        failures += Check(character == '.' && visible, "arena floor glyph is retained");

        failures += Check(
            AsciiCanvas_SetCharacter(&canvas, 2, 2, 0, '@', true),
            "runtime character can be written"
        );
        failures += Check(
            AsciiCanvas_GetCharacter(&canvas, 2, 2, 0, &character, &visible) &&
                character == '@' && visible,
            "runtime character can be read back"
        );
        failures += Check(
            !AsciiCanvas_SetCharacter(&canvas, 48, 24, 0, '@', true),
            "out-of-bounds write is rejected"
        );
    }

    BGrid_Destroy(&canvas.grid);

    if (failures == 0)
        printf("AsciiCanvasTests passed.\n");

    return failures == 0 ? 0 : 1;
}
