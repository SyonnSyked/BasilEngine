#include "BEditorWorkspaceSession.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static int Check(bool condition, const char *message)
{
    if (condition)
        return 0;

    std::fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

int main()
{
    int failures = 0;
    std::string error;
    fs::path root = fs::temp_directory_path() /
                    ("basil-workspace-session-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "workspaces");
    fs::create_directories(root / "assets" / "sprites");
    {
        std::FILE *sprite =
            std::fopen((root / "assets" / "sprites" / "ship.txt").string().c_str(), "wb");
        if (sprite != nullptr) {
            std::fputs("/\\\n<@>\n", sprite);
            std::fclose(sprite);
        }
    }
    fs::path workspacePath = root / "workspaces" / "Main.basilworkspace";
    BWorkspaceDocument initial{};
    BDiagnosticList diagnostics{};
    failures +=
        Check(BWorkspaceDocument_CreateDefault(&initial, "Main Workspace", "Main", &diagnostics),
              "fixture Workspace is created");
    failures +=
        Check(BWorkspaceDocument_Save(&initial, workspacePath.string().c_str(), &diagnostics),
              "fixture saves");
    BWorkspaceDocument_Destroy(&initial);

    BEditorWorkspaceSession session;
    BEditorWorkspaceSession escapingSession;
    failures += Check(!escapingSession.Load(root, "../Outside.basilworkspace", error),
                      "session rejects a Workspace path outside the Project");
    failures += Check(session.Load(root, "workspaces/Main.basilworkspace", error),
                      "session loads Workspace");
    failures += Check(session.IsLoaded() && !session.IsDirty(), "loaded session starts clean");
    failures += Check(!session.Load(root, "workspaces/Missing.basilworkspace", error),
                      "failed replacement load is reported");
    failures += Check(session.IsLoaded() && session.Workspace().identifier[0] != '\0',
                      "failed replacement load preserves the active Workspace");
    failures += Check(session.AddEntity(error), "session adds entity");
    failures += Check(session.IsDirty(), "adding marks session dirty");
    failures += Check(session.SelectedEntity() != nullptr, "added entity is selected");
    failures +=
        Check(session.SelectedEntity()->componentCount == 2 &&
                  BWorkspaceEntity_FindComponentConst(session.SelectedEntity(),
                                                      BWORKSPACE_TRANSFORM2D_TYPE) != nullptr &&
                  BWorkspaceEntity_FindComponentConst(session.SelectedEntity(),
                                                      BWORKSPACE_ASCII_RENDERABLE_TYPE) != nullptr,
              "normal entity is immediately visible with Transform2D and ASCII Renderable");
    failures += Check(session.DuplicateSelectedEntity(error), "selected entity duplicates");
    failures += Check(session.Workspace().entityCount == 2 && session.SelectedIndex() == 1,
                      "duplicate has a new selected entity");
    failures += Check(session.Undo(error) && session.Workspace().entityCount == 1,
                      "undo restores pre-duplicate document");
    failures += Check(session.Redo(error) && session.Workspace().entityCount == 2,
                      "redo restores duplicate");
    failures += Check(session.Undo(error) && session.Workspace().entityCount == 1,
                      "second undo returns fixture to one entity");

    BWorkspaceEntity *selected = session.MutableSelectedEntity();
    std::string originalName = selected->name;
    failures += Check(!session.SetSelectedName("", error),
                      "invalid entity name is rejected at mutation time");
    failures += Check(std::string(selected->name) == originalName,
                      "invalid name preserves the last valid value");
    failures +=
        Check(session.SetSelectedName("Edited Entity", error), "validated entity name is applied");
    failures +=
        Check(session.SetSelectedEnabled(false, error), "enabled state uses shared mutation API");
    failures += Check(session.SetSelectedTransform({12.5f, -4.25f}, error),
                      "position uses shared mutation API");
    BWorkspaceComponent *renderComponent =
        BWorkspaceEntity_FindComponent(selected, BWORKSPACE_ASCII_RENDERABLE_TYPE);
    BAsciiRenderable editedRenderable = renderComponent->data.asciiRenderable;
    editedRenderable.glyph = '#';
    editedRenderable.layer = 17;
    editedRenderable.anchor = BASCII_ANCHOR_TOP_LEFT;
    failures += Check(session.SetSelectedRenderable(editedRenderable, error),
                      "renderable fields use shared mutation API");
    BAsciiRenderable invalidRenderable = editedRenderable;
    invalidRenderable.glyph = '\t';
    failures += Check(!session.SetSelectedRenderable(invalidRenderable, error),
                      "invalid renderable edit is rejected");
    failures += Check(renderComponent->data.asciiRenderable.glyph == '#',
                      "invalid renderable preserves the last valid value");
    failures += Check(session.Save(error), "session saves edits");
    failures += Check(!session.IsDirty(), "save clears dirty state");
    session.Select(0);
    failures += Check(session.SetSelectedEnabled(true, error), "post-save mutation succeeds");
    failures += Check(session.IsDirty(), "post-save mutation is dirty");
    failures += Check(session.Undo(error), "post-save mutation can be undone");
    failures += Check(!session.IsDirty(), "undo to saved state clears dirty status");
    failures += Check(session.Redo(error) && session.IsDirty(),
                      "redo away from saved state restores dirty status");
    failures += Check(session.Undo(error), "fixture returns to saved state");
    failures += Check(session.Reload(error), "session reloads saved Workspace");
    failures += Check(session.Workspace().entityCount == 1 &&
                          std::string(session.Workspace().entities[0].name) == "Edited Entity" &&
                          !session.Workspace().entities[0].enabled,
                      "saved entity and component edits round trip");

    session.Select(0);
    failures += Check(session.RemoveSelectedEntity(error), "session removes selected entity");
    failures +=
        Check(session.Workspace().entityCount == 0, "removed entity leaves Workspace empty");
    failures += Check(session.SelectedEntity() == nullptr, "remove clears selection");
    failures +=
        Check(session.AddTextSpriteEntity("asset-ship-test", "assets/sprites/ship.txt", error),
              "session adds Text Sprite entity");
    const BWorkspaceComponent *textRenderable = BWorkspaceEntity_FindComponentConst(
        session.SelectedEntity(), BWORKSPACE_ASCII_RENDERABLE_TYPE);
    failures +=
        Check(textRenderable != nullptr &&
                  textRenderable->data.asciiRenderable.sourceKind == BASCII_SOURCE_TEXT_SPRITE &&
                  std::string(textRenderable->data.asciiRenderable.textSprite.path) ==
                      "assets/sprites/ship.txt",
              "Text Sprite entity stores a portable Project-relative asset path");
    failures +=
        Check(session.RemapAssetPath("assets/sprites/ship.txt", "assets/sprites/vessel.txt", error),
              "asset reference remaps transactionally");
    textRenderable = BWorkspaceEntity_FindComponentConst(session.SelectedEntity(),
                                                         BWORKSPACE_ASCII_RENDERABLE_TYPE);
    failures += Check(std::string(textRenderable->data.asciiRenderable.textSprite.path) ==
                          "assets/sprites/vessel.txt",
                      "remap updates matching renderable");
    failures += Check(session.Undo(error), "asset remap participates in undo");
    failures += Check(session.SetSelectedEnabled(false, error),
                      "disabled Text Sprite fixture is configured");
    {
        std::FILE *sprite =
            std::fopen((root / "assets" / "sprites" / "ship.txt").string().c_str(), "wb");
        if (sprite != nullptr) {
            std::fputs("bad\tasset\n", sprite);
            std::fclose(sprite);
        }
    }
    BDiagnosticList runDiagnostics{};
    failures += Check(!session.ValidateForRun(runDiagnostics, error),
                      "Run preflight validates disabled Text Sprite references");
    const BDiagnostic *runError = BDiagnosticList_FirstError(&runDiagnostics);
    failures +=
        Check(runError != nullptr && std::string(runError->path) == "assets/sprites/ship.txt" &&
                  std::string(runError->entityId) == session.SelectedEntity()->id,
              "Run preflight reports structured asset and entity context");
    {
        std::FILE *sprite =
            std::fopen((root / "assets" / "sprites" / "ship.txt").string().c_str(), "wb");
        if (sprite != nullptr) {
            std::fputs("/\\\n<@>\n", sprite);
            std::fclose(sprite);
        }
    }
    failures += Check(session.ValidateForRun(runDiagnostics, error),
                      "Run preflight accepts restored Text Sprite");
    failures += Check(!session.AddTextSpriteEntity("asset-escape-test", "../escape.txt", error),
                      "escaping Text Sprite path is rejected");
    failures += Check(session.Workspace().entityCount == 1,
                      "failed Text Sprite creation rolls back completely");
    failures += Check(session.AddEmptyEntity(error), "session adds explicit empty entity");
    failures +=
        Check(session.SelectedEntity()->componentCount == 1 &&
                  BWorkspaceEntity_FindComponentConst(session.SelectedEntity(),
                                                      BWORKSPACE_TRANSFORM2D_TYPE) != nullptr &&
                  BWorkspaceEntity_FindComponentConst(session.SelectedEntity(),
                                                      BWORKSPACE_ASCII_RENDERABLE_TYPE) == nullptr,
              "empty entity contains only Transform2D");
    failures += Check(session.RemoveSelectedEntity(error), "empty entity can be removed");
    session.Select(0);
    failures += Check(session.RemoveSelectedEntity(error), "Text Sprite entity can be removed");
    failures +=
        Check(session.Workspace().entityCount == 0, "authoring fixture returns to empty state");

    failures += Check(session.SaveRecovery(error), "dirty Workspace recovery saves separately");
    fs::last_write_time(fs::path(workspacePath.string() + ".recovery"),
                        fs::last_write_time(workspacePath) + std::chrono::seconds(1));
    BEditorWorkspaceSession recovered;
    failures += Check(recovered.Load(root, "workspaces/Main.basilworkspace", error),
                      "recovery fixture loads canonical Workspace");
    failures += Check(recovered.HasNewerRecovery(), "newer recovery is detected");
    failures += Check(recovered.RestoreRecovery(error), "recovery restores transactionally");
    failures += Check(recovered.IsDirty() && recovered.Workspace().entityCount == 0,
                      "restored recovery remains unsaved");
    failures += Check(recovered.DiscardRecovery(error), "recovery can be explicitly discarded");
    failures += Check(!recovered.HasNewerRecovery(), "discard removes recovery marker");

    fs::remove_all(root);

    if (failures == 0)
        std::printf("BEditorWorkspaceSessionTests passed.\n");

    return failures == 0 ? 0 : 1;
}
