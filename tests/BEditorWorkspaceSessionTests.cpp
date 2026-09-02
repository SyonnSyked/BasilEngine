#include "BEditorWorkspaceSession.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static int Check(bool condition, const char* message)
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
        ("basil-workspace-session-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ));
    fs::create_directories(root / "workspaces");
    fs::path workspacePath = root / "workspaces" / "Main.basilworkspace";
    BWorkspaceDocument initial{};
    BDiagnosticList diagnostics{};
    failures += Check(
        BWorkspaceDocument_CreateDefault(&initial, "Main Workspace", "Main", &diagnostics),
        "fixture Workspace is created"
    );
    failures += Check(
        BWorkspaceDocument_Save(&initial, workspacePath.string().c_str(), &diagnostics),
        "fixture saves"
    );
    BWorkspaceDocument_Destroy(&initial);

    BEditorWorkspaceSession session;
    BEditorWorkspaceSession escapingSession;
    failures += Check(
        !escapingSession.Load(root, "../Outside.basilworkspace", error),
        "session rejects a Workspace path outside the Project"
    );
    failures += Check(session.Load(root, "workspaces/Main.basilworkspace", error), "session loads Workspace");
    failures += Check(session.IsLoaded() && !session.IsDirty(), "loaded session starts clean");
    failures += Check(
        !session.Load(root, "workspaces/Missing.basilworkspace", error),
        "failed replacement load is reported"
    );
    failures += Check(
        session.IsLoaded() && session.Workspace().identifier[0] != '\0',
        "failed replacement load preserves the active Workspace"
    );
    failures += Check(session.AddEntity(error), "session adds entity");
    failures += Check(session.IsDirty(), "adding marks session dirty");
    failures += Check(session.SelectedEntity() != nullptr, "added entity is selected");

    BWorkspaceEntity* selected = session.MutableSelectedEntity();
    selected->name[0] = '\0';
    session.MarkDirty();
    failures += Check(!session.Save(error), "invalid entity edit is not saved");
    failures += Check(session.IsDirty(), "failed save preserves dirty state");
    std::snprintf(selected->name, sizeof(selected->name), "Edited Entity");
    session.MarkDirty();
    failures += Check(session.Save(error), "session saves edits");
    failures += Check(!session.IsDirty(), "save clears dirty state");
    failures += Check(session.Reload(error), "session reloads saved Workspace");
    failures += Check(
        session.Workspace().entityCount == 1 &&
            std::string(session.Workspace().entities[0].name) == "Edited Entity",
        "saved entity round trips"
    );

    session.Select(0);
    failures += Check(session.RemoveSelectedEntity(error), "session removes selected entity");
    failures += Check(session.Workspace().entityCount == 0, "removed entity leaves Workspace empty");
    failures += Check(session.SelectedEntity() == nullptr, "remove clears selection");

    fs::remove_all(root);

    if (failures == 0)
        std::printf("BEditorWorkspaceSessionTests passed.\n");

    return failures == 0 ? 0 : 1;
}
