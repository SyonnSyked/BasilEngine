#include "BEditorUIConfig.h"

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

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
    BEditorUIConfig config = BEditorUIConfig_Default();

    failures += Check(BEditorUIConfig_Validate(config, error), "default UI Config validates");
    failures += Check(config.showProjectDetails, "default shows Project Details");
    failures += Check(config.showWorkspaceHierarchy, "default shows Workspace Hierarchy");
    failures += Check(config.showInspector, "default shows Inspector");
    failures += Check(config.showWorkspaceViewport, "default shows Workspace Viewport");
    failures += Check(config.showAssets, "default shows Assets");
    failures += Check(config.showConsole, "default shows Console");
    failures += Check(!config.showBuildOutput, "default hides inactive Build Output");
    failures += Check(!config.showProblems, "default hides inactive Problems");
    failures += Check(!config.showTerminal, "default hides inactive Terminal");
    failures += Check(
        std::string(BEditorPanel_Name(BEditorPanel::WorkspaceHierarchy)) == "WORKSPACE HIERARCHY",
        "panel identity is stable"
    );

    config.bottomRatio = 0.9f;
    failures += Check(!BEditorUIConfig_Validate(config, error), "invalid panel ratio is rejected");
    failures += Check(!error.empty(), "invalid config provides an explanation");

    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / ("basil-ui-config-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path path = root / "custom.basilui.json";
    config = BEditorUIConfig_Default();
    config.showTerminal = true;
    config.showConsole = false;
    config.leftRatio = 0.3f;
    failures += Check(BEditorUIConfig_Save(path.string(), config, error), "UI Config saves as JSON");
    BEditorUIConfig loaded;
    failures += Check(BEditorUIConfig_Load(path.string(), loaded, error), "UI Config loads");
    failures += Check(loaded.showTerminal && !loaded.showConsole && loaded.leftRatio == 0.3f, "UI Config fields round trip");
    failures += Check(!loaded.showTextSpriteEditor, "new Text Sprite panel visibility round trips");
    config.showProblems = true;
    failures += Check(BEditorUIConfig_Save(path.string(), config, error), "UI Config replacement saves");
    failures += Check(fs::is_regular_file(path.string() + ".bak"), "UI Config replacement retains backup");
    {
        std::ofstream malformed(path, std::ios::trunc);
        malformed << "{ bad json";
    }
    loaded.showTerminal = false;
    failures += Check(!BEditorUIConfig_Load(path.string(), loaded, error), "malformed UI Config is rejected");
    failures += Check(loaded.showTerminal == BEditorUIConfig_Default().showTerminal, "failed load returns safe defaults");
    {
        std::ofstream legacy(path, std::ios::trunc);
        legacy << "{\"schemaVersion\":1,\"leftRatio\":0.22,\"rightRatio\":0.22,\"bottomRatio\":0.25,"
            "\"showProjectDetails\":true,\"showWorkspaceHierarchy\":true,\"showInspector\":true,"
            "\"showWorkspaceViewport\":true,\"showAssets\":true,\"showConsole\":true,"
            "\"showBuildOutput\":false,\"showProblems\":false,\"showTerminal\":false}";
    }
    failures += Check(BEditorUIConfig_Load(path.string(), loaded, error), "schema-1 UI Config migrates in memory");
    failures += Check(loaded.schemaVersion == BEDITOR_UI_CONFIG_SCHEMA_VERSION && !loaded.showTextSpriteEditor, "migration supplies the new panel default");
    fs::remove_all(root);

    if (failures == 0)
        std::printf("BEditorUIConfigTests passed.\n");

    return failures == 0 ? 0 : 1;
}
