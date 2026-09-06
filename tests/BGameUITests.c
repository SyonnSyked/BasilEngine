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
    BGameUIContext_BeginFrame(&ui, 20, 10, -1, -1);
    BGameUIContext_Begin(&ui, &selection, (BGameUIInput){0});
    BGameUICell panel =
        BGameUIContext_Box(&ui, (BGameUIRect){BGAME_UI_TOP_LEFT, -2, -1, 6, 4});
    failures += Check(panel.x == -2 && panel.y == -1, "box returns its resolved origin");
    BGameUIContext_Label(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 18, 9}, "ABCD");
    (void)BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 2}, "Yes");
    (void)BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 3}, "No");
    failures += Check(!BGameUIContext_End(&ui), "idle UI does not consume input");
    failures += Check(ui.commandCount > 0, "primitives create commands");
    for (size_t i = 0; i < ui.commandCount; ++i)
        failures += Check(ui.commands[i].x >= 0 && ui.commands[i].x < 20 &&
                              ui.commands[i].y >= 0 && ui.commands[i].y < 10,
                          "commands are clipped to the grid");

    BGameUIContext_BeginFrame(&ui, 20, 10, -1, -1);
    BGameUIContext_Begin(&ui, &selection,
                         (BGameUIInput){.next = true, .confirm = true});
    bool yes = BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 2}, "Yes");
    bool no = BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 3}, "No");
    failures += Check(BGameUIContext_End(&ui) && selection == 1 && !yes && no,
                      "next and confirm activate the selected choice and consume input");

    selection = 0;
    BGameUIContext_BeginFrame(&ui, 20, 10, 3, 3);
    BGameUIContext_Begin(&ui, &selection, (BGameUIInput){.pointerActivate = true});
    yes = BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 2}, "Yes");
    no = BGameUIContext_Choice(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, 2, 3}, "No");
    int markerCount = 0;
    int selectedStyleCount = 0;
    for (size_t i = 0; i < ui.commandCount; ++i) {
        if (ui.commands[i].glyph == '>')
            ++markerCount;
        if (ui.commands[i].foreground.r == 0 && ui.commands[i].foreground.g == 229 &&
            ui.commands[i].foreground.b == 255)
            ++selectedStyleCount;
    }
    failures += Check(BGameUIContext_End(&ui) && selection == 1 && !yes && no,
                      "mouse hover selects and click activates the later choice");
    failures += Check(markerCount == 1 && selectedStyleCount == 4,
                      "pointer selection leaves one marker and styles only the current choice");

    BGameUIContext_BeginFrame(&ui, 80, 30, -1, -1);
    BGameUIContext_Begin(&ui, NULL, (BGameUIInput){0});
    panel = BGameUIContext_Box(&ui, (BGameUIRect){BGAME_UI_BOTTOM, 0, 0, 40, 10});
    BGameUIContext_Label(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, panel.x + 2, panel.y + 1},
                         "Short");
    BGameUIContext_Label(&ui, (BGameUIPosition){BGAME_UI_TOP_LEFT, panel.x + 2, panel.y + 2},
                         "A much wider child");
    failures += Check(panel.x == 20 && panel.y == 20, "bottom panel resolves once");
    size_t shortStart = 400;
    size_t wideStart = 405;
    failures += Check(ui.commands[shortStart].x == panel.x + 2 &&
                          ui.commands[wideStart].x == panel.x + 2,
                      "panel-relative children share an origin regardless of text width");
    failures += Check(!ui.overflowed, "ordinary full-grid box fits command capacity");
    (void)BGameUIContext_End(&ui);

    BGameUIContext_BeginFrame(&ui, 80, 30, -1, -1);
    BGameUIContext_Begin(&ui, NULL, (BGameUIInput){0});
    (void)BGameUIContext_Box(&ui, (BGameUIRect){BGAME_UI_TOP_LEFT, 0, 0, 80, 30});
    failures += Check(ui.commandCount == 2400 && !ui.overflowed &&
                          ui.commands[0].glyph == '+' && ui.commands[79].glyph == '+' &&
                          ui.commands[2320].glyph == '+' && ui.commands[2399].glyph == '+',
                      "ordinary full-screen panel retains every border corner");

    BGameUIContext_BeginFrame(&ui, 80, 30, -1, -1);
    BGameUIContext_Begin(&ui, NULL, (BGameUIInput){0});
    (void)BGameUIContext_Box(&ui, (BGameUIRect){BGAME_UI_TOP_LEFT, -100000, -100000,
                                                200000, 200000});
    failures += Check(ui.commandCount == 2400 && !ui.overflowed,
                      "huge boxes iterate only the visible clipped grid");

    if (failures == 0)
        printf("BGameUITests passed.\n");
    return failures == 0 ? 0 : 1;
}
