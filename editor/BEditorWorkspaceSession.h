#ifndef BASIL_EDITOR_WORKSPACE_SESSION_H
#define BASIL_EDITOR_WORKSPACE_SESSION_H

#include "BWorkspace.h"
#include "BAsciiDrawList.h"

#include <cstddef>
#include <cstdint>
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
    bool AddGlyphEntity(char glyph, std::string& error);
    bool AddTextSpriteEntity(const std::string& relativePath, std::string& error);
    bool AddEmptyEntity(std::string& error);
    bool RemoveSelectedEntity(std::string& error);
    bool SetSelectedName(const std::string& name, std::string& error);
    bool SetSelectedEnabled(bool enabled, std::string& error);
    bool SetSelectedTransform(BTransform2D transform, std::string& error);
    bool SetSelectedRenderable(const BAsciiRenderable& renderable, std::string& error);
    bool ValidateForRun(BDiagnosticList& diagnostics, std::string& error) const;

    bool IsLoaded() const;
    bool IsDirty() const;
    bool RequiresMigration() const;
    std::uint64_t Revision() const;
    void MarkDirty();

    const std::filesystem::path& Path() const;
    const std::filesystem::path& ProjectRoot() const;
    const BWorkspaceDocument& Workspace() const;
    BWorkspaceDocument& MutableWorkspace();

    std::size_t SelectedIndex() const;
    void Select(std::size_t index);
    const BWorkspaceEntity* SelectedEntity() const;
    BWorkspaceEntity* MutableSelectedEntity();

private:
    BWorkspaceDocument workspace_{};
    std::filesystem::path projectRoot_;
    std::filesystem::path path_;
    std::size_t selectedIndex_ = NoSelection;
    bool loaded_ = false;
    bool dirty_ = false;
    std::uint64_t revision_ = 1;
};

#endif
