#include "BEditorWorkspaceSession.h"

#include <cstdio>

namespace
{
std::string FirstDiagnosticMessage(const BDiagnosticList& diagnostics)
{
    const BDiagnostic* diagnostic = BDiagnosticList_FirstError(&diagnostics);
    return diagnostic != nullptr ? diagnostic->message : "Workspace operation failed.";
}
}

BEditorWorkspaceSession::~BEditorWorkspaceSession()
{
    BWorkspaceDocument_Destroy(&workspace_);
}

void BEditorWorkspaceSession::Reset()
{
    BWorkspaceDocument_Destroy(&workspace_);
    path_.clear();
    selectedIndex_ = NoSelection;
    loaded_ = false;
    dirty_ = false;
}

bool BEditorWorkspaceSession::Load(
    const std::filesystem::path& projectRoot,
    const std::string& relativePath,
    std::string& error
)
{
    error.clear();
    std::filesystem::path normalizedRoot = std::filesystem::absolute(projectRoot).lexically_normal();
    std::filesystem::path candidate = (normalizedRoot / relativePath).lexically_normal();
    std::filesystem::path relativeCandidate = candidate.lexically_relative(normalizedRoot);

    if (relativePath.empty() || std::filesystem::path(relativePath).is_absolute() ||
        relativeCandidate.empty() || *relativeCandidate.begin() == "..")
    {
        error = "Workspace path must remain inside the Project root.";
        loaded_ = false;
        dirty_ = false;
        selectedIndex_ = NoSelection;
        return false;
    }
    BDiagnosticList diagnostics{};

    if (!BWorkspaceDocument_Load(candidate.string().c_str(), &workspace_, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    path_ = candidate;
    selectedIndex_ = NoSelection;
    loaded_ = true;
    dirty_ = false;
    return true;
}

bool BEditorWorkspaceSession::Reload(std::string& error)
{
    if (!loaded_)
    {
        error = "No Workspace is loaded.";
        return false;
    }

    BDiagnosticList diagnostics{};

    if (!BWorkspaceDocument_Load(path_.string().c_str(), &workspace_, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    selectedIndex_ = NoSelection;
    dirty_ = false;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::Save(std::string& error)
{
    if (!loaded_)
    {
        error = "No Workspace is loaded.";
        return false;
    }

    BDiagnosticList diagnostics{};

    if (!BWorkspaceDocument_Save(&workspace_, path_.string().c_str(), &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    dirty_ = false;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::AddEntity(std::string& error)
{
    if (!loaded_)
    {
        error = "No Workspace is loaded.";
        return false;
    }

    char name[BWORKSPACE_ENTITY_NAME_MAX];
    std::snprintf(name, sizeof(name), "Entity %llu", workspace_.nextEntityId);
    BDiagnosticList diagnostics{};
    std::size_t index = 0;

    if (!BWorkspaceDocument_AddEntity(&workspace_, name, &index, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    selectedIndex_ = index;
    dirty_ = true;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::RemoveSelectedEntity(std::string& error)
{
    if (!loaded_ || selectedIndex_ >= workspace_.entityCount)
    {
        error = "No Workspace entity is selected.";
        return false;
    }

    BDiagnosticList diagnostics{};

    if (!BWorkspaceDocument_RemoveEntity(&workspace_, selectedIndex_, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    selectedIndex_ = NoSelection;
    dirty_ = true;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::IsLoaded() const { return loaded_; }
bool BEditorWorkspaceSession::IsDirty() const { return dirty_; }
void BEditorWorkspaceSession::MarkDirty() { if (loaded_) dirty_ = true; }
const std::filesystem::path& BEditorWorkspaceSession::Path() const { return path_; }
const BWorkspaceDocument& BEditorWorkspaceSession::Workspace() const { return workspace_; }
BWorkspaceDocument& BEditorWorkspaceSession::MutableWorkspace() { return workspace_; }
std::size_t BEditorWorkspaceSession::SelectedIndex() const { return selectedIndex_; }

void BEditorWorkspaceSession::Select(std::size_t index)
{
    selectedIndex_ = loaded_ && index < workspace_.entityCount ? index : NoSelection;
}

const BWorkspaceEntity* BEditorWorkspaceSession::SelectedEntity() const
{
    return selectedIndex_ < workspace_.entityCount ? &workspace_.entities[selectedIndex_] : nullptr;
}

BWorkspaceEntity* BEditorWorkspaceSession::MutableSelectedEntity()
{
    return selectedIndex_ < workspace_.entityCount ? &workspace_.entities[selectedIndex_] : nullptr;
}
