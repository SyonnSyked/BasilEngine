#include "BGameUI.h"

#include <stdio.h>
#include <string.h>

static int Check(bool condition, const char *message)
{
    if (condition)
        return 0;
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    int failures = 0;
    BGameUICell cell = BGameUI_ResolveAnchor(BGAME_UI_TOP_LEFT, 2, 3, 10, 4, 80, 25);
    failures += Check(cell.x == 2 && cell.y == 3, "top-left anchor");
    cell = BGameUI_ResolveAnchor(BGAME_UI_CENTER, 0, 0, 10, 5, 80, 25);
    failures += Check(cell.x == 35 && cell.y == 10, "center anchor");
    cell = BGameUI_ResolveAnchor(BGAME_UI_BOTTOM_RIGHT, -2, -1, 10, 4, 80, 25);
    failures += Check(cell.x == 68 && cell.y == 20, "bottom-right anchor");
    failures += Check(BGameUI_HitTest(4, 5, 6, 1, 4, 5) &&
                          BGameUI_HitTest(4, 5, 6, 1, 9, 5) &&
                          !BGameUI_HitTest(4, 5, 6, 1, 10, 5),
                      "half-open hit testing");

    BGameUIContext ui;
    BGameUIContext_Init(&ui);
    int selection = 0;
    BGameUIContext_BeginFrame(&ui, 20, 10, -1, -1, false, false, false, false);
    BGameUIContext_Begin(&ui, &selection);
    BGameUIContext_Box(&ui, (BGameUIRect){BGAME_UI_TOP_LEFT, -2, -1, 6, 4});
    BGameUIContext_Label(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 18, 9}, "ABCD");
    (void)BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 2}, "Yes");
    (void)BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 3}, "No");
    failures += Check(!BGameUIContext_End(&ui), "idle UI does not consume input");
    failures += Check(ui.commandCount > 0, "primitives create commands");
    for (size_t i = 0; i < ui.commandCount; ++i)
        failures += Check(ui.commands[i].x >= 0 && ui.commands[i].x < 20 &&
                              ui.commands[i].y >= 0 && ui.commands[i].y < 10,
                          "commands are clipped to the grid");

    BGameUIContext_BeginFrame(&ui, 20, 10, -1, -1, false, true, true, false);
    BGameUIContext_Begin(&ui, &selection);
    bool yes = BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 2}, "Yes");
    bool no = BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 3}, "No");
    failures += Check(BGameUIContext_End(&ui) && selection == 1 && !yes && no,
                      "next and confirm activate the selected choice and consume input");

    BGameUIContext_BeginFrame(&ui, 20, 10, 3, 2, false, false, false, true);
    BGameUIContext_Begin(&ui, &selection);
    yes = BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 2}, "Yes");
    no = BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 3}, "No");
    failures += Check(BGameUIContext_End(&ui) && selection == 0 && yes && !no,
                      "mouse hover selects and click activates");

    if (failures == 0)
        printf("BGameUITests passed.\n");
    return failures == 0 ? 0 : 1;
}
