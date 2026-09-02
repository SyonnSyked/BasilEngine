#include "BEditorBuildService.h"

#include "BEditorBuildConfig.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <sstream>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{
constexpr std::size_t OUTPUT_LIMIT = 2 * 1024 * 1024;

void AppendBounded(std::string& output, const char* data, std::size_t size)
{
    output.append(data, size);

    if (output.size() > OUTPUT_LIMIT)
        output.erase(0, output.size() - OUTPUT_LIMIT);
}

#ifdef _WIN32
std::wstring Widen(const std::string& value)
{
    if (value.empty())
        return {};

    int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);

    if (length <= 0)
        return {};

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length);
    result.pop_back();
    return result;
}

std::wstring QuoteWindowsArgument(const std::wstring& argument)
{
    if (argument.find_first_of(L" \t\"") == std::wstring::npos)
        return argument;

    std::wstring result = L"\"";
    std::size_t slashes = 0;

    for (wchar_t character : argument)
    {
        if (character == L'\\')
        {
            ++slashes;
            continue;
        }

        if (character == L'\"')
            result.append(slashes * 2 + 1, L'\\');
        else
            result.append(slashes, L'\\');

        slashes = 0;
        result.push_back(character);
    }

    result.append(slashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}
#endif

class ChildProcess
{
public:
    ~ChildProcess() { Close(); }

    bool Start(
        const std::vector<std::string>& arguments,
        const fs::path& workingDirectory,
        std::string& error
    )
    {
        Close();

        if (arguments.empty())
        {
            error = "Process command is empty.";
            return false;
        }

#ifdef _WIN32
        SECURITY_ATTRIBUTES attributes{};
        attributes.nLength = sizeof(attributes);
        attributes.bInheritHandle = TRUE;
        HANDLE readPipe = INVALID_HANDLE_VALUE;
        HANDLE writePipe = INVALID_HANDLE_VALUE;

        if (!CreatePipe(&readPipe, &writePipe, &attributes, 0) ||
            !SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0))
        {
            if (readPipe != INVALID_HANDLE_VALUE)
                CloseHandle(readPipe);
            if (writePipe != INVALID_HANDLE_VALUE)
                CloseHandle(writePipe);
            error = "Could not create process output pipe.";
            return false;
        }

        std::wstring commandLine;

        for (const std::string& argument : arguments)
        {
            if (!commandLine.empty())
                commandLine.push_back(L' ');
            commandLine += QuoteWindowsArgument(Widen(argument));
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = writePipe;
        startup.hStdError = writePipe;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION process{};
        std::wstring directory = workingDirectory.wstring();
        BOOL started = CreateProcessW(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED,
            nullptr,
            directory.c_str(),
            &startup,
            &process
        );
        CloseHandle(writePipe);

        if (!started)
        {
            CloseHandle(readPipe);
            error = "Could not start process (Windows error " + std::to_string(GetLastError()) + ").";
            return false;
        }

        HANDLE job = CreateJobObjectW(nullptr, nullptr);

        if (job == nullptr)
            job = INVALID_HANDLE_VALUE;
        else
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

            if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
                !AssignProcessToJobObject(job, process.hProcess))
            {
                CloseHandle(job);
                job = INVALID_HANDLE_VALUE;
            }
        }

        ResumeThread(process.hThread);
        CloseHandle(process.hThread);
        processHandle_ = process.hProcess;
        processId_ = process.dwProcessId;
        readPipe_ = readPipe;
        jobHandle_ = job;
#else
        int pipeHandles[2];

        if (pipe(pipeHandles) != 0)
        {
            error = "Could not create process output pipe: " + std::string(std::strerror(errno));
            return false;
        }

        pid_t child = fork();

        if (child == 0)
        {
            setpgid(0, 0);
            chdir(workingDirectory.string().c_str());
            dup2(pipeHandles[1], STDOUT_FILENO);
            dup2(pipeHandles[1], STDERR_FILENO);
            close(pipeHandles[0]);
            close(pipeHandles[1]);
            std::vector<char*> values;
            values.reserve(arguments.size() + 1);

            for (const std::string& argument : arguments)
                values.push_back(const_cast<char*>(argument.c_str()));

            values.push_back(nullptr);
            execvp(values[0], values.data());
            _exit(127);
        }

        close(pipeHandles[1]);

        if (child < 0)
        {
            close(pipeHandles[0]);
            error = "Could not start process: " + std::string(std::strerror(errno));
            return false;
        }

        int flags = fcntl(pipeHandles[0], F_GETFL, 0);
        fcntl(pipeHandles[0], F_SETFL, flags | O_NONBLOCK);
        processId_ = child;
        setpgid(child, child);
        readPipe_ = pipeHandles[0];
#endif
        return true;
    }

    void Read(std::string& output)
    {
#ifdef _WIN32
        if (readPipe_ == INVALID_HANDLE_VALUE)
            return;

        for (;;)
        {
            DWORD available = 0;

            if (!PeekNamedPipe(readPipe_, nullptr, 0, nullptr, &available, nullptr) || available == 0)
                break;

            std::array<char, 4096> buffer{};
            DWORD read = 0;

            if (!ReadFile(readPipe_, buffer.data(), std::min<DWORD>(available, buffer.size()), &read, nullptr) || read == 0)
                break;

            AppendBounded(output, buffer.data(), read);
        }
#else
        if (readPipe_ < 0)
            return;

        std::array<char, 4096> buffer{};

        for (;;)
        {
            ssize_t count = read(readPipe_, buffer.data(), buffer.size());

            if (count <= 0)
                break;

            AppendBounded(output, buffer.data(), static_cast<std::size_t>(count));
        }
#endif
    }

    bool Poll(int& exitCode)
    {
#ifdef _WIN32
        if (processHandle_ == INVALID_HANDLE_VALUE)
            return false;

        DWORD code = STILL_ACTIVE;

        if (!GetExitCodeProcess(processHandle_, &code) || code == STILL_ACTIVE)
            return false;

        exitCode = static_cast<int>(code);
#else
        if (processId_ <= 0)
            return false;

        int status = 0;
        pid_t result = waitpid(processId_, &status, WNOHANG);

        if (result <= 0)
            return false;

        exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
#endif
        return true;
    }

    bool Pause(bool pause, std::string& error)
    {
#ifdef _WIN32
        if (processHandle_ == INVALID_HANDLE_VALUE)
        {
            error = "No process is active.";
            return false;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

        if (snapshot == INVALID_HANDLE_VALUE)
        {
            error = "Could not inspect game threads.";
            return false;
        }

        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        bool changed = false;

        if (Thread32First(snapshot, &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID != processId_)
                    continue;

                HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);

                if (thread != nullptr)
                {
                    DWORD result = pause ? SuspendThread(thread) : ResumeThread(thread);
                    changed = changed || result != static_cast<DWORD>(-1);
                    CloseHandle(thread);
                }
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);

        if (!changed)
            error = pause ? "Could not pause the game process." : "Could not resume the game process.";

        return changed;
#else
        if (processId_ <= 0 || kill(-processId_, pause ? SIGSTOP : SIGCONT) != 0)
        {
            error = pause ? "Could not pause the game process." : "Could not resume the game process.";
            return false;
        }

        return true;
#endif
    }

    bool Stop(std::string& error)
    {
#ifdef _WIN32
        if (processHandle_ == INVALID_HANDLE_VALUE ||
            !(jobHandle_ != INVALID_HANDLE_VALUE ? TerminateJobObject(jobHandle_, 1) : TerminateProcess(processHandle_, 1)))
#else
        if (processId_ <= 0 || kill(-processId_, SIGTERM) != 0)
#endif
        {
            error = "Could not stop the active process.";
            return false;
        }

#ifdef _WIN32
        WaitForSingleObject(processHandle_, 5000);
#else
        waitpid(processId_, nullptr, 0);
#endif

        return true;
    }

    void Close()
    {
#ifdef _WIN32
        if (readPipe_ != INVALID_HANDLE_VALUE)
            CloseHandle(readPipe_);
        if (processHandle_ != INVALID_HANDLE_VALUE)
            CloseHandle(processHandle_);
        if (jobHandle_ != INVALID_HANDLE_VALUE)
            CloseHandle(jobHandle_);
        readPipe_ = INVALID_HANDLE_VALUE;
        processHandle_ = INVALID_HANDLE_VALUE;
        jobHandle_ = INVALID_HANDLE_VALUE;
        processId_ = 0;
#else
        if (readPipe_ >= 0)
            close(readPipe_);
        readPipe_ = -1;
        processId_ = -1;
#endif
    }

private:
#ifdef _WIN32
    HANDLE processHandle_ = INVALID_HANDLE_VALUE;
    HANDLE jobHandle_ = INVALID_HANDLE_VALUE;
    HANDLE readPipe_ = INVALID_HANDLE_VALUE;
    DWORD processId_ = 0;
#else
    pid_t processId_ = -1;
    int readPipe_ = -1;
#endif
};

void AddCacheArgument(std::vector<std::string>& arguments, const char* name, const char* value)
{
    if (value != nullptr && value[0] != '\0')
        arguments.emplace_back(std::string("-D") + name + "=" + value);
}
}

class BEditorBuildService::Implementation
{
public:
    ~Implementation()
    {
        if (IsBusy())
        {
            std::string ignored;
            process.Stop(ignored);
        }
    }

    bool StartBuild(const fs::path& root, const BProject& value, bool run, std::string& error)
    {
        if (IsBusy())
        {
            error = "A build or game process is already active.";
            return false;
        }

        projectRoot = root;
        project = value;
        runAfterBuild = run;
        output.clear();
        problems.clear();
        output = std::string("> CONFIGURE ") + project.name + "\n";
        std::vector<std::string> arguments{
            BEDITOR_CMAKE_COMMAND,
            "-S", projectRoot.string(),
            "-B", (projectRoot / "build").string(),
            "-G", BEDITOR_CMAKE_GENERATOR
        };
        AddCacheArgument(arguments, "CMAKE_BUILD_TYPE", BEDITOR_CMAKE_BUILD_TYPE);
        AddCacheArgument(arguments, "CMAKE_TOOLCHAIN_FILE", BEDITOR_CMAKE_TOOLCHAIN_FILE);
        AddCacheArgument(arguments, "CMAKE_C_COMPILER", BEDITOR_C_COMPILER);
        AddCacheArgument(arguments, "CMAKE_CXX_COMPILER", BEDITOR_CXX_COMPILER);
        AddCacheArgument(arguments, "BASIL_ENGINE_ROOT", BEDITOR_ENGINE_SOURCE_DIR);
        AddCacheArgument(arguments, "BASIL_RAYLIB_ROOT", BEDITOR_RAYLIB_ROOT);
        AddCacheArgument(arguments, "BASIL_TOOLS_ROOT", BEDITOR_TOOLS_ROOT);

        if (!process.Start(arguments, projectRoot, error))
        {
            state = BEditorBuildState::Failed;
            return false;
        }

        state = BEditorBuildState::Configuring;
        return true;
    }

    void Update()
    {
        if (!IsBusy())
            return;

        process.Read(output);
        int exitCode = 0;

        if (!process.Poll(exitCode))
        {
            RefreshProblems();
            return;
        }

        process.Read(output);
        process.Close();

        if (state == BEditorBuildState::Configuring)
        {
            if (exitCode != 0)
            {
                Fail("Configuration failed with exit code " + std::to_string(exitCode) + ".");
                return;
            }

            output += "\n> BUILD\n";
            std::vector<std::string> arguments{
                BEDITOR_CMAKE_COMMAND,
                "--build", (projectRoot / "build").string()
            };

            if (BEDITOR_CMAKE_BUILD_TYPE[0] != '\0')
            {
                arguments.emplace_back("--config");
                arguments.emplace_back(BEDITOR_CMAKE_BUILD_TYPE);
            }

            std::string error;

            if (!process.Start(arguments, projectRoot, error))
            {
                Fail(error);
                return;
            }

            state = BEditorBuildState::Building;
            return;
        }

        if (state == BEditorBuildState::Building)
        {
            if (exitCode != 0)
            {
                Fail("Build failed with exit code " + std::to_string(exitCode) + ".");
                return;
            }

            output += "\n> BUILD SUCCEEDED\n";
            state = BEditorBuildState::BuildSucceeded;

            if (runAfterBuild)
                StartGame();

            RefreshProblems();
            return;
        }

        if (state == BEditorBuildState::Running || state == BEditorBuildState::Paused)
        {
            output += "\n> GAME EXITED WITH CODE " + std::to_string(exitCode) + "\n";
            state = exitCode == 0 ? BEditorBuildState::Completed : BEditorBuildState::Failed;
            RefreshProblems();
        }
    }

    bool Pause(bool pause, std::string& error)
    {
        BEditorBuildState required = pause ? BEditorBuildState::Running : BEditorBuildState::Paused;

        if (state != required)
        {
            error = pause ? "The game is not running." : "The game is not paused.";
            return false;
        }

        if (!process.Pause(pause, error))
            return false;

        state = pause ? BEditorBuildState::Paused : BEditorBuildState::Running;
        output += pause ? "\n> GAME PAUSED\n" : "\n> GAME RESUMED\n";
        return true;
    }

    bool Stop(std::string& error)
    {
        if (!IsBusy())
        {
            error = "No build or game process is active.";
            return false;
        }

        if (!process.Stop(error))
            return false;

        process.Close();
        output += "\n> PROCESS STOPPED\n";
        state = BEditorBuildState::Completed;
        return true;
    }

    bool IsBusy() const
    {
        return state == BEditorBuildState::Configuring ||
            state == BEditorBuildState::Building ||
            state == BEditorBuildState::Running ||
            state == BEditorBuildState::Paused;
    }

    bool IsGameActive() const
    {
        return state == BEditorBuildState::Running || state == BEditorBuildState::Paused;
    }

    void StartGame()
    {
        std::vector<fs::path> candidates{
#ifdef _WIN32
            projectRoot / "build" / (std::string(project.identifier) + ".exe"),
            projectRoot / "build" / BEDITOR_CMAKE_BUILD_TYPE / (std::string(project.identifier) + ".exe")
#else
            projectRoot / "build" / project.identifier,
            projectRoot / "build" / BEDITOR_CMAKE_BUILD_TYPE / project.identifier
#endif
        };
        auto executable = std::find_if(candidates.begin(), candidates.end(), [](const fs::path& path)
        {
            std::error_code error;
            return fs::is_regular_file(path, error);
        });

        if (executable == candidates.end())
        {
            Fail("Build succeeded, but the Project executable could not be located.");
            return;
        }

        output += "\n> RUN " + executable->string() + "\n";
        std::string error;

        if (!process.Start({ executable->string() }, projectRoot, error))
        {
            Fail(error);
            return;
        }

        state = BEditorBuildState::Running;
    }

    void Fail(const std::string& message)
    {
        output += "\n[ERROR] " + message + "\n";
        state = BEditorBuildState::Failed;
        RefreshProblems();
    }

    void RefreshProblems()
    {
        problems.clear();
        std::istringstream lines(output);
        std::string line;

        while (std::getline(lines, line))
        {
            std::string lowered = line;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });

            if (lowered.find("error:") != std::string::npos ||
                lowered.find("error c") != std::string::npos ||
                lowered.find("cmake error") != std::string::npos ||
                lowered.find("fatal error") != std::string::npos ||
                lowered.find("undefined reference") != std::string::npos ||
                lowered.find("[error]") != std::string::npos)
            {
                problems.push_back(line);
            }
        }
    }

    ChildProcess process;
    BEditorBuildState state = BEditorBuildState::Idle;
    fs::path projectRoot;
    BProject project{};
    bool runAfterBuild = false;
    std::string output;
    std::vector<std::string> problems;
};

BEditorBuildService::BEditorBuildService() : implementation_(std::make_unique<Implementation>()) {}
BEditorBuildService::~BEditorBuildService() = default;

bool BEditorBuildService::StartBuild(const fs::path& root, const BProject& project, bool run, std::string& error)
{
    return implementation_->StartBuild(root, project, run, error);
}

void BEditorBuildService::Update() { implementation_->Update(); }
bool BEditorBuildService::Pause(std::string& error) { return implementation_->Pause(true, error); }
bool BEditorBuildService::Resume(std::string& error) { return implementation_->Pause(false, error); }
bool BEditorBuildService::Stop(std::string& error) { return implementation_->Stop(error); }
BEditorBuildState BEditorBuildService::State() const { return implementation_->state; }
bool BEditorBuildService::IsBusy() const { return implementation_->IsBusy(); }
bool BEditorBuildService::IsGameActive() const { return implementation_->IsGameActive(); }
const std::string& BEditorBuildService::Output() const { return implementation_->output; }
const std::vector<std::string>& BEditorBuildService::Problems() const { return implementation_->problems; }

const char* BEditorBuildService::StateLabel() const
{
    switch (implementation_->state)
    {
        case BEditorBuildState::Idle: return "IDLE";
        case BEditorBuildState::Configuring: return "CONFIGURING";
        case BEditorBuildState::Building: return "BUILDING";
        case BEditorBuildState::BuildSucceeded: return "BUILD SUCCEEDED";
        case BEditorBuildState::Running: return "RUNNING";
        case BEditorBuildState::Paused: return "PAUSED";
        case BEditorBuildState::Completed: return "STOPPED";
        case BEditorBuildState::Failed: return "FAILED";
    }

    return "UNKNOWN";
}
