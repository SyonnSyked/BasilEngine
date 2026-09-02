#include "BEditorBuildService.h"

#include "BProjectGenerator.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

int main()
{
    int failures = 0;
    fs::path parent = fs::temp_directory_path() /
        ("basil-build-service-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ));
    fs::create_directories(parent);
    BProject project = BProject_Default("Build Service Test", "BuildServiceTest");
    BProjectError projectError{};
    failures += Check(
        BProjectGenerator_Create(&project, parent.string().c_str(), &projectError),
        "test Project is generated"
    );

    fs::path projectRoot = parent / project.identifier;
    {
        std::ofstream source(projectRoot / "source" / "main.c", std::ios::binary);
        source <<
            "#ifdef _WIN32\n"
            "#include <windows.h>\n"
            "int main(void) { Sleep(30000); return 0; }\n"
            "#else\n"
            "#include <unistd.h>\n"
            "int main(void) { sleep(30); return 0; }\n"
            "#endif\n";
        failures += Check(source.good(), "headless run fixture is written");
    }

    BEditorBuildService service;
    std::string error;
    failures += Check(service.StartBuild(projectRoot, project, false, error), "asynchronous build starts");

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);

    while (service.IsBusy() && std::chrono::steady_clock::now() < deadline)
    {
        service.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    service.Update();
    failures += Check(!service.IsBusy(), "build finishes before timeout");
    failures += Check(service.State() == BEditorBuildState::BuildSucceeded, "build reports success");
    failures += Check(service.Output().find("BUILD SUCCEEDED") != std::string::npos, "success is captured in output");
    failures += Check(service.Problems().empty(), "successful build has no problems");

#ifdef _WIN32
    fs::path executable = projectRoot / "build" / "BuildServiceTest.exe";
#else
    fs::path executable = projectRoot / "build" / "BuildServiceTest";
#endif
    failures += Check(fs::is_regular_file(executable), "build produces Project executable");

    failures += Check(service.StartBuild(projectRoot, project, true, error), "build-and-run starts");
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);

    while (service.State() != BEditorBuildState::Running &&
        service.IsBusy() && std::chrono::steady_clock::now() < deadline)
    {
        service.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    failures += Check(service.State() == BEditorBuildState::Running, "game process reaches running state");
    failures += Check(service.Pause(error), "running game can be paused");
    failures += Check(service.State() == BEditorBuildState::Paused, "pause state is reported");
    failures += Check(service.Resume(error), "paused game can be resumed");
    failures += Check(service.State() == BEditorBuildState::Running, "resume state is reported");
    failures += Check(service.Stop(error), "running game can be stopped");
    failures += Check(service.State() == BEditorBuildState::Completed, "stop state is reported");

    {
        std::ofstream source(projectRoot / "source" / "main.c", std::ios::binary);
        source << "int main(void) { this_will_not_compile return 0; }\n";
    }

    failures += Check(service.StartBuild(projectRoot, project, false, error), "failing build starts");
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);

    while (service.IsBusy() && std::chrono::steady_clock::now() < deadline)
    {
        service.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    service.Update();
    failures += Check(service.State() == BEditorBuildState::Failed, "compiler failure is reported");
    failures += Check(!service.Problems().empty(), "compiler failure populates Problems");
    failures += Check(service.StartBuild(projectRoot, project, false, error), "cancellable build starts");
    failures += Check(service.Stop(error), "configuration process tree can be stopped");
    failures += Check(service.State() == BEditorBuildState::Completed, "cancelled build reports stopped state");
    fs::remove_all(parent);

    if (failures == 0)
        std::printf("BEditorBuildServiceTests passed.\n");

    return failures == 0 ? 0 : 1;
}
