#ifndef BASIL_EDITOR_UI_CONFIG_H
#define BASIL_EDITOR_UI_CONFIG_H

#include <string>

constexpr int BEDITOR_UI_CONFIG_SCHEMA_VERSION = 1;

enum class BEditorPanel
{
    ProjectDetails,
    WorkspaceHierarchy,
    Inspector,
    WorkspaceViewport,
    Assets,
    Console,
    BuildOutput,
    Problems,
    Terminal
};

struct BEditorUIConfig
{
    int schemaVersion = BEDITOR_UI_CONFIG_SCHEMA_VERSION;
    float leftRatio = 0.22f;
    float rightRatio = 0.22f;
    float bottomRatio = 0.25f;
    bool showProjectDetails = true;
    bool showWorkspaceHierarchy = true;
    bool showInspector = true;
    bool showWorkspaceViewport = true;
    bool showAssets = true;
    bool showConsole = true;
    bool showBuildOutput = false;
    bool showProblems = false;
    bool showTerminal = false;
};

BEditorUIConfig BEditorUIConfig_Default();
bool BEditorUIConfig_Validate(const BEditorUIConfig& config, std::string& error);
const char* BEditorPanel_Name(BEditorPanel panel);

#endif
