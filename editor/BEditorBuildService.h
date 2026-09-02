#ifndef BASIL_EDITOR_BUILD_SERVICE_H
#define BASIL_EDITOR_BUILD_SERVICE_H

#include "BProject.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

enum class BEditorBuildState
{
    Idle,
    Configuring,
    Building,
    BuildSucceeded,
    Running,
    Paused,
    Completed,
    Failed
};

class BEditorBuildService
{
public:
    BEditorBuildService();
    ~BEditorBuildService();
    BEditorBuildService(const BEditorBuildService&) = delete;
    BEditorBuildService& operator=(const BEditorBuildService&) = delete;

    bool StartBuild(
        const std::filesystem::path& projectRoot,
        const std::filesystem::path& manifestPath,
        const BProject& project,
        bool runAfterBuild,
        std::string& error
    );
    void ReportPreflightFailure(const std::vector<std::string>& diagnostics);
    void Update();
    bool Pause(std::string& error);
    bool Resume(std::string& error);
    bool Stop(std::string& error);

    BEditorBuildState State() const;
    bool IsBusy() const;
    bool IsGameActive() const;
    const std::string& Output() const;
    const std::vector<std::string>& Problems() const;
    const char* StateLabel() const;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

#endif
