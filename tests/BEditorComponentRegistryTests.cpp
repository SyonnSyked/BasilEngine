#include "BEditorComponentRegistry.h"
extern "C"
{
#include "BWorkspace.h"
}

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static int Check(bool value, const char* message) { if (value) return 0; std::fprintf(stderr, "FAILED: %s\n", message); return 1; }
static void Write(const fs::path& path, const char* value) { fs::create_directories(path.parent_path()); std::ofstream(path) << value; }

int main()
{
    int failures = 0; std::string error;
    fs::path root = fs::temp_directory_path() / ("basil-components-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Write(root / ".basil/components.json", R"({"schemaVersion":1,"types":[{"id":"game.health","displayName":"Health","version":1,"fields":[{"id":"current","displayName":"Current","type":"int","default":100},{"id":"tint","displayName":"Tint","type":"color","default":"#00E5FFFF"},{"id":"faction","displayName":"Faction","type":"enum","default":"neutral","options":["neutral","hostile"]}]}]})");
    BEditorComponentRegistry registry;
    failures += Check(registry.Open(root, error), "valid metadata loads without Project code");
    failures += Check(registry.Types().size() == 1 && registry.Types()[0].fields.size() == 3, "typed metadata is available");
    std::string defaults; failures += Check(registry.DefaultDataJson("game.health", defaults, error), "typed defaults are created");
    BWorkspaceDocument workspace; BWorkspaceDocument_Init(&workspace); BDiagnosticList diagnostics{}; size_t entity = 0;
    failures += Check(BWorkspaceDocument_CreateDefault(&workspace, "Test", "Test", &diagnostics), "Workspace created");
    failures += Check(BWorkspaceDocument_AddEntity(&workspace, "Player", &entity, &diagnostics), "entity created");
    failures += Check(BWorkspaceDocument_AddCustomComponentJson(&workspace, entity, "game.health", 1, defaults.c_str(), &diagnostics), "custom component added");
    fs::path workspacePath = root / "workspaces/Main.basilworkspace";
    fs::create_directories(workspacePath.parent_path());
    failures += Check(BWorkspaceDocument_Save(&workspace, workspacePath.string().c_str(), &diagnostics), "custom data saves");
    BWorkspaceDocument loaded; BWorkspaceDocument_Init(&loaded);
    failures += Check(BWorkspaceDocument_Load(workspacePath.string().c_str(), &loaded, &diagnostics), "custom data reloads");
    const BWorkspaceComponent* component = BWorkspaceEntity_FindComponentConst(&loaded.entities[0], "game.health");
    failures += Check(component && component->kind == BWORKSPACE_COMPONENT_UNKNOWN && std::string(component->data.unknownDataJson) == defaults, "custom data round-trips exactly");
    Write(root / ".basil/components.json", R"({"schemaVersion":1,"types":[{"id":"bad","displayName":"Bad","version":1,"fields":[{"id":"value","displayName":"Value","type":"int","default":"wrong"}]}]})");
    failures += Check(!registry.Open(root, error), "mismatched field default is rejected");
    BWorkspaceDocument_Destroy(&loaded); BWorkspaceDocument_Destroy(&workspace); fs::remove_all(root);
    if (!failures)
        std::printf("BEditorComponentRegistryTests passed.\n");
    return failures ? 1 : 0;
}
