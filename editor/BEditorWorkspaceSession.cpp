#include "BEditorWorkspaceSession.h"

#include <cstdio>
#include <system_error>

namespace
{
std::string FirstDiagnosticMessage(const BDiagnosticList& diagnostics)
{
    const BDiagnostic* diagnostic = BDiagnosticList_FirstError(&diagnostics);
    return diagnostic != nullptr ? diagnostic->message : "Workspace operation failed.";
}
}

struct BEditorWorkspaceSession::Snapshot
{
    BWorkspaceDocument workspace{};
    std::size_t selectedIndex = NoSelection;
    std::uint64_t stateId = 0;
    ~Snapshot() { BWorkspaceDocument_Destroy(&workspace); }
};

bool BEditorWorkspaceSession::Capture(
    std::vector<std::unique_ptr<Snapshot>>& target,
    std::string& error
) const
{
    auto snapshot = std::make_unique<Snapshot>();
    BDiagnosticList diagnostics{};
    if (!BWorkspaceDocument_Clone(&workspace_, &snapshot->workspace, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    snapshot->selectedIndex = selectedIndex_;
    snapshot->stateId = stateId_;
    target.push_back(std::move(snapshot));
    if (target.size() > 128)
        target.erase(target.begin());
    return true;
}

bool BEditorWorkspaceSession::Restore(std::unique_ptr<Snapshot> snapshot, std::string& error)
{
    BDiagnosticList diagnostics{};
    if (!snapshot || !BWorkspaceDocument_Clone(&snapshot->workspace, &workspace_, &diagnostics))
    {
        error = snapshot ? FirstDiagnosticMessage(diagnostics) : "History entry is unavailable.";
        return false;
    }
    selectedIndex_ = snapshot->selectedIndex < workspace_.entityCount ? snapshot->selectedIndex : NoSelection;
    stateId_ = snapshot->stateId;
    dirty_ = stateId_ != savedStateId_;
    ++revision_;
    error.clear();
    return true;
}

void BEditorWorkspaceSession::CommitMutation()
{
    redo_.clear();
    stateId_++;
    dirty_ = stateId_ != savedStateId_;
    ++revision_;
}

void BEditorWorkspaceSession::CancelMutation()
{
    if (!undo_.empty())
        undo_.pop_back();
}

std::filesystem::path BEditorWorkspaceSession::RecoveryPath() const
{
    return path_.empty() ? std::filesystem::path{} : std::filesystem::path(path_.string() + ".recovery");
}

BEditorWorkspaceSession::BEditorWorkspaceSession() = default;

BEditorWorkspaceSession::~BEditorWorkspaceSession()
{
    BWorkspaceDocument_Destroy(&workspace_);
}

void BEditorWorkspaceSession::Reset()
{
    BWorkspaceDocument_Destroy(&workspace_);
    path_.clear();
    projectRoot_.clear();
    selectedIndex_ = NoSelection;
    loaded_ = false;
    dirty_ = false;
    undo_.clear();
    redo_.clear();
    stateId_ = savedStateId_ = 1;
    ++revision_;
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
    projectRoot_ = normalizedRoot;
    selectedIndex_ = NoSelection;
    loaded_ = true;
    dirty_ = false;
    undo_.clear();
    redo_.clear();
    stateId_ = savedStateId_ = 1;
    ++revision_;
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
    undo_.clear();
    redo_.clear();
    stateId_ = savedStateId_ = 1;
    ++revision_;
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

    workspace_.sourceSchemaVersion = BWORKSPACE_SCHEMA_VERSION;
    dirty_ = false;
    savedStateId_ = stateId_;
    std::error_code removeError;
    std::filesystem::remove(RecoveryPath(), removeError);
    removeError.clear();
    std::filesystem::remove(std::filesystem::path(RecoveryPath().string() + ".bak"), removeError);
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SaveRecovery(std::string& error)
{
    if (!loaded_ || !dirty_) { error = "No modified Workspace is available for recovery."; return false; }
    BDiagnosticList diagnostics{};
    if (!BWorkspaceDocument_Save(&workspace_, RecoveryPath().string().c_str(), &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::HasNewerRecovery() const
{
    if (!loaded_) return false;
    std::error_code error;
    const auto recovery = RecoveryPath();
    if (!std::filesystem::is_regular_file(recovery, error)) return false;
    auto recoveryTime = std::filesystem::last_write_time(recovery, error);
    if (error) return false;
    auto sourceTime = std::filesystem::last_write_time(path_, error);
    return error || recoveryTime > sourceTime;
}

bool BEditorWorkspaceSession::RestoreRecovery(std::string& error)
{
    if (!HasNewerRecovery()) { error = "No newer Workspace recovery is available."; return false; }
    if (!Capture(undo_, error)) return false;
    BDiagnosticList diagnostics{};
    if (!BWorkspaceDocument_Load(RecoveryPath().string().c_str(), &workspace_, &diagnostics))
    {
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    selectedIndex_ = NoSelection;
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::DiscardRecovery(std::string& error)
{
    std::error_code removeError;
    std::filesystem::remove(RecoveryPath(), removeError);
    std::filesystem::remove(std::filesystem::path(RecoveryPath().string() + ".bak"), removeError);
    if (removeError) { error = "Could not discard Workspace recovery: " + removeError.message(); return false; }
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::AddEntity(std::string& error)
{
    return AddGlyphEntity('@', error);
}

bool BEditorWorkspaceSession::AddGlyphEntity(char glyph, std::string& error)
{
    if (!loaded_)
    {
        error = "No Workspace is loaded.";
        return false;
    }
    if (!Capture(undo_, error)) return false;

    char name[BWORKSPACE_ENTITY_NAME_MAX];
    std::snprintf(name, sizeof(name), "Entity %llu", workspace_.nextEntityId);
    BDiagnosticList diagnostics{};
    std::size_t index = 0;

    if (!BWorkspaceDocument_AddEntity(&workspace_, name, &index, &diagnostics))
    {
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    BAsciiRenderable renderable = BAsciiRenderable_DefaultGlyph(glyph);
    if (!BWorkspaceDocument_AddTransform2D(&workspace_, index, BTransform2D{ 0.0f, 0.0f }, true, &diagnostics) ||
        !BWorkspaceDocument_AddAsciiRenderable(&workspace_, index, &renderable, true, &diagnostics))
    {
        BWorkspaceDocument_RemoveEntity(&workspace_, index, nullptr);
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    selectedIndex_ = index;
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::AddTextSpriteEntity(const std::string& relativePath, std::string& error)
{
    if (!loaded_)
    {
        error = "No Workspace is loaded.";
        return false;
    }
    if (relativePath.empty() || relativePath.size() >= BWORKSPACE_PATH_MAX)
    {
        error = "Text Sprite path is empty or too long.";
        return false;
    }
    if (!Capture(undo_, error)) return false;
    char name[BWORKSPACE_ENTITY_NAME_MAX];
    std::snprintf(name, sizeof(name), "Text Sprite %llu", workspace_.nextEntityId);
    BDiagnosticList diagnostics{};
    std::size_t index = 0;
    BAsciiRenderable renderable = BAsciiRenderable_DefaultGlyph('@');
    renderable.sourceKind = BASCII_SOURCE_TEXT_SPRITE;
    std::snprintf(renderable.textSpritePath, sizeof(renderable.textSpritePath), "%s", relativePath.c_str());
    if (!BWorkspaceDocument_AddEntity(&workspace_, name, &index, &diagnostics))
    {
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    if (!BWorkspaceDocument_AddTransform2D(&workspace_, index, BTransform2D{ 0.0f, 0.0f }, true, &diagnostics) ||
        !BWorkspaceDocument_AddAsciiRenderable(&workspace_, index, &renderable, true, &diagnostics))
    {
        BWorkspaceDocument_RemoveEntity(&workspace_, index, nullptr);
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    selectedIndex_ = index;
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::AddEmptyEntity(std::string& error)
{
    if (!loaded_)
    {
        error = "No Workspace is loaded.";
        return false;
    }
    if (!Capture(undo_, error)) return false;
    char name[BWORKSPACE_ENTITY_NAME_MAX];
    std::snprintf(name, sizeof(name), "Empty Entity %llu", workspace_.nextEntityId);
    BDiagnosticList diagnostics{};
    std::size_t index = 0;
    if (!BWorkspaceDocument_AddEntity(&workspace_, name, &index, &diagnostics))
    {
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    if (!BWorkspaceDocument_AddTransform2D(&workspace_, index, BTransform2D{ 0.0f, 0.0f }, true, &diagnostics))
    {
        BWorkspaceDocument_RemoveEntity(&workspace_, index, nullptr);
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    selectedIndex_ = index;
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SetSelectedName(const std::string& name, std::string& error)
{
    if (selectedIndex_ >= workspace_.entityCount) { error = "No Workspace entity is selected."; return false; }
    if (!Capture(undo_, error)) return false;
    BDiagnosticList diagnostics{};
    if (selectedIndex_ >= workspace_.entityCount ||
        !BWorkspaceDocument_SetEntityName(&workspace_, selectedIndex_, name.c_str(), &diagnostics))
    {
        CancelMutation();
        error = selectedIndex_ >= workspace_.entityCount ? "No Workspace entity is selected." : FirstDiagnosticMessage(diagnostics);
        return false;
    }
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SetSelectedEnabled(bool enabled, std::string& error)
{
    if (selectedIndex_ >= workspace_.entityCount) { error = "No Workspace entity is selected."; return false; }
    if (!Capture(undo_, error)) return false;
    BDiagnosticList diagnostics{};
    if (selectedIndex_ >= workspace_.entityCount ||
        !BWorkspaceDocument_SetEntityEnabled(&workspace_, selectedIndex_, enabled, &diagnostics))
    {
        CancelMutation();
        error = selectedIndex_ >= workspace_.entityCount ? "No Workspace entity is selected." : FirstDiagnosticMessage(diagnostics);
        return false;
    }
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SetSelectedTransform(BTransform2D transform, std::string& error)
{
    if (selectedIndex_ >= workspace_.entityCount) { error = "No Workspace entity is selected."; return false; }
    if (!Capture(undo_, error)) return false;
    BDiagnosticList diagnostics{};
    if (selectedIndex_ >= workspace_.entityCount ||
        !BWorkspaceDocument_SetTransform2D(&workspace_, selectedIndex_, transform, &diagnostics))
    {
        CancelMutation();
        error = selectedIndex_ >= workspace_.entityCount ? "No Workspace entity is selected." : FirstDiagnosticMessage(diagnostics);
        return false;
    }
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SetSelectedRenderable(const BAsciiRenderable& renderable, std::string& error)
{
    if (selectedIndex_ >= workspace_.entityCount) { error = "No Workspace entity is selected."; return false; }
    if (!Capture(undo_, error)) return false;
    BDiagnosticList diagnostics{};
    if (selectedIndex_ >= workspace_.entityCount ||
        !BWorkspaceDocument_SetAsciiRenderable(&workspace_, selectedIndex_, &renderable, &diagnostics))
    {
        CancelMutation();
        error = selectedIndex_ >= workspace_.entityCount ? "No Workspace entity is selected." : FirstDiagnosticMessage(diagnostics);
        return false;
    }
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::ValidateForRun(BDiagnosticList& diagnostics, std::string& error) const
{
    if (!loaded_)
    {
        BDiagnosticList_Clear(&diagnostics);
        BDiagnosticList_Add(&diagnostics, BDIAGNOSTIC_ERROR, BDIAGNOSTIC_INVALID_ARGUMENT, "No Workspace is loaded.", nullptr);
        error = "No Workspace is loaded.";
        return false;
    }
    BTextSpriteCache cache;
    BTextSpriteCache_Init(&cache);
    bool succeeded = BWorkspaceDocument_ValidateTextSprites(
        &workspace_, projectRoot_.string().c_str(), &cache, &diagnostics
    );
    BTextSpriteCache_Destroy(&cache);
    error = succeeded ? std::string{} : FirstDiagnosticMessage(diagnostics);
    return succeeded;
}

bool BEditorWorkspaceSession::RemoveSelectedEntity(std::string& error)
{
    if (!loaded_ || selectedIndex_ >= workspace_.entityCount)
    {
        error = "No Workspace entity is selected.";
        return false;
    }
    if (!Capture(undo_, error)) return false;

    BDiagnosticList diagnostics{};

    if (!BWorkspaceDocument_RemoveEntity(&workspace_, selectedIndex_, &diagnostics))
    {
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    selectedIndex_ = NoSelection;
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::DuplicateSelectedEntity(std::string& error)
{
    if (!loaded_ || selectedIndex_ >= workspace_.entityCount)
    {
        error = "No Workspace entity is selected.";
        return false;
    }
    if (!Capture(undo_, error)) return false;
    BDiagnosticList diagnostics{};
    std::size_t duplicateIndex = NoSelection;
    if (!BWorkspaceDocument_DuplicateEntity(&workspace_, selectedIndex_, &duplicateIndex, &diagnostics))
    {
        CancelMutation();
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    selectedIndex_ = duplicateIndex;
    CommitMutation();
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::RemapAssetPath(const std::string& oldPath, const std::string& newPath, std::string& error)
{
    if (!loaded_ || oldPath.empty() || newPath.empty() || newPath.size() >= BWORKSPACE_PATH_MAX)
    { error = "Valid old and new Project asset paths are required."; return false; }
    std::vector<std::size_t> matches;
    for (std::size_t i = 0; i < workspace_.entityCount; ++i)
    {
        BWorkspaceComponent* component = BWorkspaceEntity_FindComponent(&workspace_.entities[i], BWORKSPACE_ASCII_RENDERABLE_TYPE);
        if (component && component->data.asciiRenderable.sourceKind == BASCII_SOURCE_TEXT_SPRITE && oldPath == component->data.asciiRenderable.textSpritePath)
            matches.push_back(i);
    }
    if (matches.empty()) { error.clear(); return true; }
    if (!Capture(undo_, error)) return false;
    BDiagnosticList diagnostics{};
    for (std::size_t index : matches)
    {
        BWorkspaceComponent* component = BWorkspaceEntity_FindComponent(&workspace_.entities[index], BWORKSPACE_ASCII_RENDERABLE_TYPE);
        BAsciiRenderable updated = component->data.asciiRenderable;
        std::snprintf(updated.textSpritePath, sizeof(updated.textSpritePath), "%s", newPath.c_str());
        if (!BWorkspaceDocument_SetAsciiRenderable(&workspace_, index, &updated, &diagnostics))
        {
            auto snapshot = std::move(undo_.back()); undo_.pop_back();
            Restore(std::move(snapshot), error);
            error = FirstDiagnosticMessage(diagnostics);
            return false;
        }
    }
    CommitMutation(); error.clear(); return true;
}

bool BEditorWorkspaceSession::Undo(std::string& error)
{
    if (undo_.empty()) { error = "Nothing to undo."; return false; }
    if (!Capture(redo_, error)) return false;
    auto snapshot = std::move(undo_.back());
    undo_.pop_back();
    if (!Restore(std::move(snapshot), error))
    {
        redo_.pop_back();
        return false;
    }
    return true;
}

bool BEditorWorkspaceSession::Redo(std::string& error)
{
    if (redo_.empty()) { error = "Nothing to redo."; return false; }
    if (!Capture(undo_, error)) return false;
    auto snapshot = std::move(redo_.back());
    redo_.pop_back();
    if (!Restore(std::move(snapshot), error))
    {
        undo_.pop_back();
        return false;
    }
    return true;
}

bool BEditorWorkspaceSession::CanUndo() const { return !undo_.empty(); }
bool BEditorWorkspaceSession::CanRedo() const { return !redo_.empty(); }

bool BEditorWorkspaceSession::IsLoaded() const { return loaded_; }
bool BEditorWorkspaceSession::IsDirty() const { return dirty_; }
bool BEditorWorkspaceSession::RequiresMigration() const
{
    return loaded_ && BWorkspaceDocument_RequiresMigration(&workspace_);
}
std::uint64_t BEditorWorkspaceSession::Revision() const { return revision_; }
void BEditorWorkspaceSession::MarkDirty() { if (loaded_) { CommitMutation(); } }
const std::filesystem::path& BEditorWorkspaceSession::Path() const { return path_; }
const std::filesystem::path& BEditorWorkspaceSession::ProjectRoot() const { return projectRoot_; }
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
