#ifndef BASIL_EDITOR_UI_CONFIG_H
#define BASIL_EDITOR_UI_CONFIG_H

#include <string>

constexpr int BEDITOR_UI_CONFIG_SCHEMA_VERSION = 2;

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
    Terminal,
    TextSpriteEditor,
    CodeEditor
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
    bool showTextSpriteEditor = false;
    bool showCodeEditor = true;
};

BEditorUIConfig BEditorUIConfig_Default();
bool BEditorUIConfig_Validate(const BEditorUIConfig& config, std::string& error);
bool BEditorUIConfig_Load(const std::string& path, BEditorUIConfig& output, std::string& error);
bool BEditorUIConfig_Save(const std::string& path, const BEditorUIConfig& config, std::string& error);
const char* BEditorPanel_Name(BEditorPanel panel);

#endif
