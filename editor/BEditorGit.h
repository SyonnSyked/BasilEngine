#ifndef BASIL_EDITOR_GIT_H
#define BASIL_EDITOR_GIT_H

#include <filesystem>

// A Project is initialized only when its own root contains Git metadata. A
// parent repository does not count. Both .git directories and worktree-style
// .git files are supported.
bool BEditorGit_IsInitialized(const std::filesystem::path& projectRoot);
bool BEditorGit_Initialize(const std::filesystem::path& projectRoot);

#endif
