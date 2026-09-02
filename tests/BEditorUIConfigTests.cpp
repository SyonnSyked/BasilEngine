#include "BEditorUIConfig.h"

#include <cstdio>
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

    if (failures == 0)
        std::printf("BEditorUIConfigTests passed.\n");

    return failures == 0 ? 0 : 1;
}
