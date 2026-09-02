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
    projectRoot_.clear();
    selectedIndex_ = NoSelection;
    loaded_ = false;
    dirty_ = false;
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

    char name[BWORKSPACE_ENTITY_NAME_MAX];
    std::snprintf(name, sizeof(name), "Entity %llu", workspace_.nextEntityId);
    BDiagnosticList diagnostics{};
    std::size_t index = 0;

    if (!BWorkspaceDocument_AddEntity(&workspace_, name, &index, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    BAsciiRenderable renderable = BAsciiRenderable_DefaultGlyph(glyph);
    if (!BWorkspaceDocument_AddTransform2D(&workspace_, index, BTransform2D{ 0.0f, 0.0f }, true, &diagnostics) ||
        !BWorkspaceDocument_AddAsciiRenderable(&workspace_, index, &renderable, true, &diagnostics))
    {
        BWorkspaceDocument_RemoveEntity(&workspace_, index, nullptr);
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    selectedIndex_ = index;
    dirty_ = true;
    ++revision_;
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
    char name[BWORKSPACE_ENTITY_NAME_MAX];
    std::snprintf(name, sizeof(name), "Text Sprite %llu", workspace_.nextEntityId);
    BDiagnosticList diagnostics{};
    std::size_t index = 0;
    BAsciiRenderable renderable = BAsciiRenderable_DefaultGlyph('@');
    renderable.sourceKind = BASCII_SOURCE_TEXT_SPRITE;
    std::snprintf(renderable.textSpritePath, sizeof(renderable.textSpritePath), "%s", relativePath.c_str());
    if (!BWorkspaceDocument_AddEntity(&workspace_, name, &index, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    if (!BWorkspaceDocument_AddTransform2D(&workspace_, index, BTransform2D{ 0.0f, 0.0f }, true, &diagnostics) ||
        !BWorkspaceDocument_AddAsciiRenderable(&workspace_, index, &renderable, true, &diagnostics))
    {
        BWorkspaceDocument_RemoveEntity(&workspace_, index, nullptr);
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    selectedIndex_ = index;
    dirty_ = true;
    ++revision_;
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
    char name[BWORKSPACE_ENTITY_NAME_MAX];
    std::snprintf(name, sizeof(name), "Empty Entity %llu", workspace_.nextEntityId);
    BDiagnosticList diagnostics{};
    std::size_t index = 0;
    if (!BWorkspaceDocument_AddEntity(&workspace_, name, &index, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    if (!BWorkspaceDocument_AddTransform2D(&workspace_, index, BTransform2D{ 0.0f, 0.0f }, true, &diagnostics))
    {
        BWorkspaceDocument_RemoveEntity(&workspace_, index, nullptr);
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }
    selectedIndex_ = index;
    dirty_ = true;
    ++revision_;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SetSelectedName(const std::string& name, std::string& error)
{
    BDiagnosticList diagnostics{};
    if (selectedIndex_ >= workspace_.entityCount ||
        !BWorkspaceDocument_SetEntityName(&workspace_, selectedIndex_, name.c_str(), &diagnostics))
    {
        error = selectedIndex_ >= workspace_.entityCount ? "No Workspace entity is selected." : FirstDiagnosticMessage(diagnostics);
        return false;
    }
    dirty_ = true;
    ++revision_;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SetSelectedEnabled(bool enabled, std::string& error)
{
    BDiagnosticList diagnostics{};
    if (selectedIndex_ >= workspace_.entityCount ||
        !BWorkspaceDocument_SetEntityEnabled(&workspace_, selectedIndex_, enabled, &diagnostics))
    {
        error = selectedIndex_ >= workspace_.entityCount ? "No Workspace entity is selected." : FirstDiagnosticMessage(diagnostics);
        return false;
    }
    dirty_ = true;
    ++revision_;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SetSelectedTransform(BTransform2D transform, std::string& error)
{
    BDiagnosticList diagnostics{};
    if (selectedIndex_ >= workspace_.entityCount ||
        !BWorkspaceDocument_SetTransform2D(&workspace_, selectedIndex_, transform, &diagnostics))
    {
        error = selectedIndex_ >= workspace_.entityCount ? "No Workspace entity is selected." : FirstDiagnosticMessage(diagnostics);
        return false;
    }
    dirty_ = true;
    ++revision_;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::SetSelectedRenderable(const BAsciiRenderable& renderable, std::string& error)
{
    BDiagnosticList diagnostics{};
    if (selectedIndex_ >= workspace_.entityCount ||
        !BWorkspaceDocument_SetAsciiRenderable(&workspace_, selectedIndex_, &renderable, &diagnostics))
    {
        error = selectedIndex_ >= workspace_.entityCount ? "No Workspace entity is selected." : FirstDiagnosticMessage(diagnostics);
        return false;
    }
    dirty_ = true;
    ++revision_;
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

    BDiagnosticList diagnostics{};

    if (!BWorkspaceDocument_RemoveEntity(&workspace_, selectedIndex_, &diagnostics))
    {
        error = FirstDiagnosticMessage(diagnostics);
        return false;
    }

    selectedIndex_ = NoSelection;
    dirty_ = true;
    ++revision_;
    error.clear();
    return true;
}

bool BEditorWorkspaceSession::IsLoaded() const { return loaded_; }
bool BEditorWorkspaceSession::IsDirty() const { return dirty_; }
bool BEditorWorkspaceSession::RequiresMigration() const
{
    return loaded_ && BWorkspaceDocument_RequiresMigration(&workspace_);
}
std::uint64_t BEditorWorkspaceSession::Revision() const { return revision_; }
void BEditorWorkspaceSession::MarkDirty() { if (loaded_) { dirty_ = true; ++revision_; } }
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
