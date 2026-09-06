#include "BGameUI.h"

#include <string.h>

static const BGameUIColor kForeground = {230, 237, 243, 255};
static const BGameUIColor kBackground = {18, 12, 31, 230};
static const BGameUIColor kSelected = {0, 229, 255, 255};

static bool ColorEquals(BGameUIColor left, BGameUIColor right)
{
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

static void Put(BGameUIContext *ui, int x, int y, char glyph, BGameUIColor foreground,
                BGameUIColor background)
{
    if (ui == NULL || x < 0 || y < 0 || x >= ui->columns || y >= ui->rows ||
        ui->commandCount >= BGAME_UI_COMMAND_CAPACITY)
        return;
    ui->commands[ui->commandCount++] =
        (BGameUIGlyphCommand){x, y, glyph, foreground, background};
}

BGameUICell BGameUI_ResolveAnchor(BGameUIAnchor anchor, int offsetX, int offsetY, int width,
                                  int height, int columns, int rows)
{
    int x = 0;
    int y = 0;
    int column = (int)anchor % 3;
    int row = (int)anchor / 3;
    if (column == 1)
        x = (columns - width) / 2;
    else if (column == 2)
        x = columns - width;
    if (row == 1)
        y = (rows - height) / 2;
    else if (row == 2)
        y = rows - height;
    return (BGameUICell){x + offsetX, y + offsetY};
}

bool BGameUI_HitTest(int x, int y, int width, int height, int pointX, int pointY)
{
    return width > 0 && height > 0 && pointX >= x && pointX < x + width && pointY >= y &&
           pointY < y + height;
}

void BGameUIContext_Init(BGameUIContext *ui)
{
    if (ui != NULL)
        memset(ui, 0, sizeof(*ui));
}

void BGameUIContext_BeginFrame(BGameUIContext *ui, int columns, int rows, int mouseX, int mouseY,
                               bool movePrevious, bool moveNext, bool confirm, bool mousePressed)
{
    if (ui == NULL)
        return;
    ui->columns = columns > 0 ? columns : 0;
    ui->rows = rows > 0 ? rows : 0;
    ui->mouseX = mouseX;
    ui->mouseY = mouseY;
    ui->movePrevious = movePrevious;
    ui->moveNext = moveNext;
    ui->confirm = confirm;
    ui->mousePressed = mousePressed;
    ui->consumed = false;
    ui->active = false;
    ui->commandCount = 0;
    ui->selectableCount = 0;
}

void BGameUIContext_Begin(BGameUIContext *ui, int *selection)
{
    if (ui == NULL || ui->active)
        return;
    ui->active = true;
    ui->selection = selection;
    if (selection == NULL)
        return;
    int count = ui->previousSelectableCount;
    if (count > 0) {
        if (*selection < 0 || *selection >= count)
            *selection = 0;
        if (ui->movePrevious) {
            *selection = (*selection + count - 1) % count;
            ui->consumed = true;
        }
        if (ui->moveNext) {
            *selection = (*selection + 1) % count;
            ui->consumed = true;
        }
    }
}

void BGameUIContext_Label(BGameUIContext *ui, BGameUIPosition position, const char *text)
{
    if (ui == NULL || !ui->active || text == NULL)
        return;
    int width = (int)strlen(text);
    BGameUICell cell = BGameUI_ResolveAnchor(position.anchor, position.x, position.y, width, 1,
                                             ui->columns, ui->rows);
    for (int i = 0; text[i] != '\0'; ++i)
        Put(ui, cell.x + i, cell.y, text[i], kForeground, (BGameUIColor){0, 0, 0, 0});
}

void BGameUIContext_Box(BGameUIContext *ui, BGameUIRect rect)
{
    if (ui == NULL || !ui->active || rect.width < 2 || rect.height < 2)
        return;
    BGameUICell cell = BGameUI_ResolveAnchor(rect.anchor, rect.x, rect.y, rect.width, rect.height,
                                             ui->columns, ui->rows);
    for (int y = 0; y < rect.height; ++y) {
        for (int x = 0; x < rect.width; ++x) {
            char glyph = ' ';
            if ((x == 0 || x == rect.width - 1) && (y == 0 || y == rect.height - 1))
                glyph = '+';
            else if (y == 0 || y == rect.height - 1)
                glyph = '-';
            else if (x == 0 || x == rect.width - 1)
                glyph = '|';
            Put(ui, cell.x + x, cell.y + y, glyph, kForeground, kBackground);
        }
    }
}

bool BGameUIContext_Choice(BGameUIContext *ui, BGameUIPosition position, const char *text)
{
    if (ui == NULL || !ui->active || text == NULL)
        return false;
    int index = ui->selectableCount++;
    int width = (int)strlen(text) + 2;
    BGameUICell cell = BGameUI_ResolveAnchor(position.anchor, position.x, position.y, width, 1,
                                             ui->columns, ui->rows);
    bool hovered = BGameUI_HitTest(cell.x, cell.y, width, 1, ui->mouseX, ui->mouseY);
    if (hovered && ui->selection != NULL) {
        if (*ui->selection != index) {
            for (size_t i = 0; i < ui->commandCount; ++i)
                if (ColorEquals(ui->commands[i].foreground, kSelected))
                    ui->commands[i].foreground = kForeground;
        }
        *ui->selection = index;
    }
    bool selected = ui->selection != NULL && *ui->selection == index;
    BGameUIColor foreground = selected ? kSelected : kForeground;
    Put(ui, cell.x, cell.y, selected ? '>' : ' ', foreground, kBackground);
    Put(ui, cell.x + 1, cell.y, ' ', foreground, kBackground);
    for (int i = 0; text[i] != '\0'; ++i)
        Put(ui, cell.x + i + 2, cell.y, text[i], foreground, kBackground);
    bool activated = (selected && ui->confirm) || (hovered && ui->mousePressed);
    if (activated)
        ui->consumed = true;
    return activated;
}

bool BGameUIContext_End(BGameUIContext *ui)
{
    if (ui == NULL || !ui->active)
        return false;
    ui->active = false;
    ui->previousSelectableCount = ui->selectableCount;
    if (ui->selection != NULL && ui->selectableCount > 0 &&
        (*ui->selection < 0 || *ui->selection >= ui->selectableCount))
        *ui->selection = 0;
    if (ui->confirm && ui->selectableCount > 0)
        ui->consumed = true;
    return ui->consumed;
}
