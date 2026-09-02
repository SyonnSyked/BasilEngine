#include "BEditorGit.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

int main()
{
    int failures = 0;
    auto unique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    fs::path root = fs::temp_directory_path() / ("basil-editor-git-" + std::to_string(unique));
    fs::path project = root / "Project";
    std::error_code error;
    fs::create_directories(project, error);
    failures += Check(!error, "test project directory is created");
    failures += Check(!BEditorGit_IsInitialized(project), "plain project is not initialized");

    fs::create_directory(project / ".git", error);
    failures += Check(!error, "Git metadata directory is created");
    failures += Check(BEditorGit_IsInitialized(project), "Git metadata directory is detected");
    fs::remove_all(project / ".git", error);

    {
        std::ofstream marker(project / ".git");
        marker << "gitdir: ../worktrees/Project\n";
    }

    failures += Check(BEditorGit_IsInitialized(project), "worktree Git marker file is detected");

    fs::path child = project / "Child";
    fs::create_directory(child, error);
    failures += Check(!BEditorGit_IsInitialized(child), "parent repository does not initialize child Project");
    fs::remove_all(root, error);

    if (failures == 0)
        std::cout << "BEditorGitTests passed.\n";

    return failures == 0 ? 0 : 1;
}
