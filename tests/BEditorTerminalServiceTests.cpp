#include "BEditorTerminalService.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>
static int Check(bool value, const char* message) { if (value) return 0; std::fprintf(stderr, "FAILED: %s\n", message); return 1; }
int main()
{
    int failures = 0; std::string error; BEditorTerminalService terminal;
#ifdef _WIN32
    const char* shell = "powershell.exe"; const char* command = "Write-Output BASIL_TERMINAL_READY";
#else
    const char* shell = "/bin/sh"; const char* command = "echo BASIL_TERMINAL_READY";
#endif
    failures += Check(terminal.Start(shell, std::filesystem::temp_directory_path(), error), "terminal starts");
    failures += Check(terminal.Send(command, error), "terminal accepts input");
    for (int i = 0; i < 100 && terminal.Output().find("BASIL_TERMINAL_READY") == std::string::npos; ++i) { terminal.Update(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
    failures += Check(terminal.Output().find("BASIL_TERMINAL_READY") != std::string::npos, "terminal captures output");
    terminal.Clear(); failures += Check(terminal.Output().empty(), "terminal clears output"); failures += Check(terminal.Restart(error), "terminal restarts"); terminal.Stop(); failures += Check(!terminal.IsRunning(), "terminal stops");
    if (!failures)
        std::printf("BEditorTerminalServiceTests passed.\n");
    return failures ? 1 : 0;
}
