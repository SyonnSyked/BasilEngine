#include "BEditorGit.h"

#include <cstdlib>

namespace fs = std::filesystem;

bool BEditorGit_IsInitialized(const fs::path& projectRoot)
{
    std::error_code error;
    fs::file_status status = fs::status(projectRoot / ".git", error);

    if (error)
        return false;

    return fs::is_directory(status) || fs::is_regular_file(status);
}

bool BEditorGit_Initialize(const fs::path& projectRoot)
{
    std::error_code error;
    fs::path previous = fs::current_path(error);

    if (error)
        return false;

    fs::current_path(projectRoot, error);

    if (error)
        return false;

    int result = std::system("git init");
    fs::current_path(previous, error);
    return result == 0 && !error && BEditorGit_IsInitialized(projectRoot);
}
