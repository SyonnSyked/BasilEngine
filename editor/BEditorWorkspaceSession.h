#ifndef BASIL_EDITOR_WORKSPACE_SESSION_H
#define BASIL_EDITOR_WORKSPACE_SESSION_H

#include "BWorkspace.h"

#include <cstddef>
#include <filesystem>
#include <string>

class BEditorWorkspaceSession
{
public:
    static constexpr std::size_t NoSelection = static_cast<std::size_t>(-1);

    BEditorWorkspaceSession() = default;
    ~BEditorWorkspaceSession();
    BEditorWorkspaceSession(const BEditorWorkspaceSession&) = delete;
    BEditorWorkspaceSession& operator=(const BEditorWorkspaceSession&) = delete;

    void Reset();

    bool Load(
        const std::filesystem::path& projectRoot,
        const std::string& relativePath,
        std::string& error
    );
    bool Reload(std::string& error);
    bool Save(std::string& error);
    bool AddEntity(std::string& error);
    bool RemoveSelectedEntity(std::string& error);

    bool IsLoaded() const;
    bool IsDirty() const;
    bool RequiresMigration() const;
    void MarkDirty();

    const std::filesystem::path& Path() const;
    const BWorkspaceDocument& Workspace() const;
    BWorkspaceDocument& MutableWorkspace();

    std::size_t SelectedIndex() const;
    void Select(std::size_t index);
    const BWorkspaceEntity* SelectedEntity() const;
    BWorkspaceEntity* MutableSelectedEntity();

private:
    BWorkspaceDocument workspace_{};
    std::filesystem::path path_;
    std::size_t selectedIndex_ = NoSelection;
    bool loaded_ = false;
    bool dirty_ = false;
};

#endif
