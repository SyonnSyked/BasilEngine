#include <string.h>
#include "BConsole.h"
#include "BLog.h"
#include "BTime.h"
#include "BEngine.h"
#include "raylib.h"

static bool g_ConsoleOpen = false;

#define BCONSOLE_INPUT_MAX 128

static char g_InputBuffer[BCONSOLE_INPUT_MAX];
static int g_InputLength = 0;

void BConsole_Init()
{
    g_ConsoleOpen = false;
    BLog_Info("Console initialized.");
}
static void BConsole_ExecuteCommand(const char* command)
{
    if (command == NULL || command[0] == '\0')
    {
        return;
    }

    BLog_InfoF("] %s", command);

    if (strcmp(command, "help") == 0)
    {
        BLog_Info("Commands: help, clear, fps, time, frame, close");
    }
    else if (strcmp(command, "clear") == 0)
    {
        BLog_Clear();
    }
    else if (strcmp(command, "fps") == 0)
    {
        BLog_InfoF("FPS: %d", BTime_GetFPS());
    }
    else if (strcmp(command, "time") == 0)
    {
        BLog_InfoF("Runtime: %.2f seconds", BTime_GetTime());
    }
    else if (strcmp(command, "frame") == 0)
    {
        BLog_InfoF("Frame: %llu", BTime_GetFrameCount());
    }
    else if (strcmp(command, "close") == 0)
    {
        g_ConsoleOpen = false;
    }
    else if (strncmp(command, "echo ", 5) == 0)
    {
        const char* message = command + 5;

        if (message[0] == '\0')
        {
            BLog_Info("Usage: echo <message>");
            return;
        }

        BLog_InfoF("%s", message);
    }
    else if (strcmp(command, "quit") == 0)
    {
        BLog_Info("Quit requested from console...");
        BEngine_RequestQuit();
    }
    else
    {
        BLog_WarningF("Unknown command: %s", command);
    }
}

static void BConsole_ClearInput() {
    g_InputBuffer[0] = '\0';
    g_InputLength = 0;
}

void BConsole_Update()
{
    if (IsKeyPressed(KEY_GRAVE))
    {
        g_ConsoleOpen = !g_ConsoleOpen;

        if (g_ConsoleOpen)
        {
            BLog_Info("Console opened.");
        }
        else
        {
            BLog_Info("Console closed.");
        }
    }

        if (!g_ConsoleOpen)
    {
        return;
    }

    int key = GetCharPressed();

    while (key > 0)
    {
        if (key >= 32 && key <= 126)
        {
            if (g_InputLength < BCONSOLE_INPUT_MAX - 1)
            {
                g_InputBuffer[g_InputLength] = (char)key;
                g_InputLength++;
                g_InputBuffer[g_InputLength] = '\0';
            }
        }

        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        if (g_InputLength > 0)
        {
            g_InputLength--;
            g_InputBuffer[g_InputLength] = '\0';
        }
    }

    if (IsKeyPressed(KEY_ENTER))
    {
        BConsole_ExecuteCommand(g_InputBuffer);
        BConsole_ClearInput();
    }
}

void BConsole_Draw()
{
    if (!g_ConsoleOpen)
    {
        return;
    }

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    int consoleHeight = screenHeight / 2;

    DrawRectangle(0, 0, screenWidth, consoleHeight, Fade(BLACK, 0.85f));
    DrawRectangleLines(0, 0, screenWidth, consoleHeight, DARKGRAY);

    DrawText("BasilEngine Console", 12, 10, 20, RAYWHITE);

    const BLogEntry* entries = BLog_GetEntries();
    size_t count = BLog_GetEntryCount();

    int y = 40;
    int lineHeight = 18;

    size_t start = 0;

    if (count > 20)
    {
        start = count - 20;
    }

    for (size_t i = start; i < count; i++)
    {
        Color color = RAYWHITE;

        if (entries[i].level == BLOG_LEVEL_WARNING)
        {
            color = YELLOW;
        }
        else if (entries[i].level == BLOG_LEVEL_ERROR)
        {
            color = RED;
        }
        else if (entries[i].level == BLOG_LEVEL_DEBUG)
        {
            color = SKYBLUE;
        }

        DrawText(entries[i].message, 12, y, 16, color);
        y += lineHeight;
    }

    int inputY = consoleHeight - 30;

    DrawRectangle(8, inputY - 4, screenWidth - 16, 24, Fade(DARKGRAY, 0.7f));
    DrawText("]", 14, inputY, 16, RAYWHITE);
    DrawText(g_InputBuffer, 34, inputY, 16, RAYWHITE);

}

void BConsole_Shutdown()
{
    BLog_Info("Console shutdown.");
}

bool BConsole_IsOpen()
{
    return g_ConsoleOpen;
}
