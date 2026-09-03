#include "BEditorTerminalService.h"

#include <array>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace
{
constexpr std::size_t OUTPUT_LIMIT = 2 * 1024 * 1024;
void Append(std::string& output, const char* data, std::size_t size) { output.append(data, size); if (output.size() > OUTPUT_LIMIT) output.erase(0, output.size() - OUTPUT_LIMIT); }
#ifdef _WIN32
std::wstring Widen(const std::string& value) { int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0); if (length <= 0) return {}; std::wstring result(static_cast<std::size_t>(length), L'\0'); MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length); result.pop_back(); return result; }
#endif
}

class BEditorTerminalService::Implementation
{
public:
    ~Implementation() { Stop(); }
    bool Start(const std::string& requestedShell, const fs::path& root, std::string& error)
    {
        Stop(); shell = requestedShell; projectRoot = root; output.clear();
        if (shell.empty() || !fs::is_directory(projectRoot)) { error = "Terminal shell and Project root are required."; return false; }
#ifdef _WIN32
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE}; HANDLE childOutWrite = INVALID_HANDLE_VALUE, childInRead = INVALID_HANDLE_VALUE;
        if (!CreatePipe(&outputRead, &childOutWrite, &attributes, 0) || !SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0) ||
            !CreatePipe(&childInRead, &inputWrite, &attributes, 0) || !SetHandleInformation(inputWrite, HANDLE_FLAG_INHERIT, 0))
        { if (childOutWrite != INVALID_HANDLE_VALUE) CloseHandle(childOutWrite); if (childInRead != INVALID_HANDLE_VALUE) CloseHandle(childInRead); Stop(); error = "Could not create terminal pipes."; return false; }
        std::wstring command = L"\"" + Widen(shell) + L"\" -NoLogo -NoProfile -NoExit";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES; startup.hStdOutput = childOutWrite; startup.hStdError = childOutWrite; startup.hStdInput = childInRead;
        PROCESS_INFORMATION process{}; std::wstring directory = projectRoot.wstring();
        BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process);
        CloseHandle(childOutWrite); CloseHandle(childInRead);
        if (!started) { Stop(); error = "Could not start terminal process (Windows error " + std::to_string(GetLastError()) + ")."; return false; }
        processHandle = process.hProcess; CloseHandle(process.hThread);
#else
        int outPipe[2], inPipe[2]; if (pipe(outPipe) || pipe(inPipe)) { error = "Could not create terminal pipes."; return false; }
        processId = fork(); if (processId == 0) { chdir(projectRoot.string().c_str()); dup2(inPipe[0], STDIN_FILENO); dup2(outPipe[1], STDOUT_FILENO); dup2(outPipe[1], STDERR_FILENO); close(outPipe[0]); close(outPipe[1]); close(inPipe[0]); close(inPipe[1]); execlp(shell.c_str(), shell.c_str(), "-i", nullptr); _exit(127); }
        close(outPipe[1]); close(inPipe[0]); outputRead = outPipe[0]; inputWrite = inPipe[1]; fcntl(outputRead, F_SETFL, fcntl(outputRead, F_GETFL, 0) | O_NONBLOCK);
        if (processId < 0) { Stop(); error = "Could not start terminal process."; return false; }
#endif
        running = true; output = "[BASIL TERMINAL] Project root: " + projectRoot.string() + "\n"; error.clear(); return true;
    }
    bool Send(const std::string& value, std::string& error)
    {
        if (!running || value.empty()) { error = "Terminal is not running or command is empty."; return false; }
        std::string line = value + "\r\n";
#ifdef _WIN32
        DWORD written = 0; if (!WriteFile(inputWrite, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) || written != line.size()) { error = "Could not write to terminal."; return false; }
#else
        if (write(inputWrite, line.data(), line.size()) != static_cast<ssize_t>(line.size())) { error = "Could not write to terminal."; return false; }
#endif
        std::string echoed = "> " + value + "\n";
        Append(output, echoed.data(), echoed.size()); error.clear(); return true;
    }
    void Update()
    {
        if (!running) return;
        for (;;) { std::array<char, 4096> buffer{};
#ifdef _WIN32
            DWORD available = 0, count = 0; if (!PeekNamedPipe(outputRead, nullptr, 0, nullptr, &available, nullptr) || !available || !ReadFile(outputRead, buffer.data(), static_cast<DWORD>(std::min<std::size_t>(available, buffer.size())), &count, nullptr) || !count) break;
#else
            ssize_t readCount = read(outputRead, buffer.data(), buffer.size()); if (readCount <= 0) break; std::size_t count = static_cast<std::size_t>(readCount);
#endif
            Append(output, buffer.data(), static_cast<std::size_t>(count)); }
#ifdef _WIN32
        DWORD code = STILL_ACTIVE; if (!GetExitCodeProcess(processHandle, &code) || code != STILL_ACTIVE) running = false;
#else
        int status = 0; if (waitpid(processId, &status, WNOHANG) > 0) running = false;
#endif
    }
    void Stop()
    {
        if (running) { std::string ignored; Send("exit", ignored);
#ifdef _WIN32
            if (WaitForSingleObject(processHandle, 1000) == WAIT_TIMEOUT) TerminateProcess(processHandle, 1);
#else
            kill(processId, SIGTERM); waitpid(processId, nullptr, 0);
#endif
        }
#ifdef _WIN32
        if (outputRead != INVALID_HANDLE_VALUE) CloseHandle(outputRead);
        if (inputWrite != INVALID_HANDLE_VALUE) CloseHandle(inputWrite);
        if (processHandle != INVALID_HANDLE_VALUE) CloseHandle(processHandle);
        outputRead = inputWrite = processHandle = INVALID_HANDLE_VALUE;
#else
        if (outputRead >= 0) close(outputRead); if (inputWrite >= 0) close(inputWrite); outputRead = inputWrite = -1; processId = -1;
#endif
        running = false;
    }
    std::string shell, output; fs::path projectRoot; bool running = false;
#ifdef _WIN32
    HANDLE outputRead = INVALID_HANDLE_VALUE, inputWrite = INVALID_HANDLE_VALUE, processHandle = INVALID_HANDLE_VALUE;
#else
    int outputRead = -1, inputWrite = -1; pid_t processId = -1;
#endif
};

BEditorTerminalService::BEditorTerminalService() : implementation_(std::make_unique<Implementation>()) {}
BEditorTerminalService::~BEditorTerminalService() = default;
bool BEditorTerminalService::Start(const std::string& shell, const fs::path& root, std::string& error) { return implementation_->Start(shell, root, error); }
bool BEditorTerminalService::Send(const std::string& command, std::string& error) { return implementation_->Send(command, error); }
bool BEditorTerminalService::Restart(std::string& error) { return implementation_->Start(implementation_->shell, implementation_->projectRoot, error); }
void BEditorTerminalService::Update() { implementation_->Update(); }
void BEditorTerminalService::Clear() { implementation_->output.clear(); }
void BEditorTerminalService::Stop() { implementation_->Stop(); }
bool BEditorTerminalService::IsRunning() const { return implementation_->running; }
const std::string& BEditorTerminalService::Output() const { return implementation_->output; }
