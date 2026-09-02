#include "BEditorUIConfig.h"

BEditorUIConfig BEditorUIConfig_Default()
{
    return BEditorUIConfig{};
}

bool BEditorUIConfig_Validate(const BEditorUIConfig& config, std::string& error)
{
    error.clear();

    if (config.schemaVersion != BEDITOR_UI_CONFIG_SCHEMA_VERSION)
    {
        error = "UI Config uses an unsupported schema version.";
        return false;
    }

    if (config.leftRatio < 0.1f || config.leftRatio > 0.4f ||
        config.rightRatio < 0.1f || config.rightRatio > 0.4f ||
        config.bottomRatio < 0.1f || config.bottomRatio > 0.45f)
    {
        error = "UI Config contains an unsupported panel ratio.";
        return false;
    }

    return true;
}

const char* BEditorPanel_Name(BEditorPanel panel)
{
    switch (panel)
    {
        case BEditorPanel::ProjectDetails: return "PROJECT DETAILS";
        case BEditorPanel::WorkspaceHierarchy: return "WORKSPACE HIERARCHY";
        case BEditorPanel::Inspector: return "INSPECTOR";
        case BEditorPanel::WorkspaceViewport: return "WORKSPACE VIEWPORT";
        case BEditorPanel::Assets: return "ASSETS";
        case BEditorPanel::Console: return "CONSOLE";
        case BEditorPanel::BuildOutput: return "BUILD OUTPUT";
        case BEditorPanel::Problems: return "PROBLEMS";
        case BEditorPanel::Terminal: return "TERMINAL";
    }

    return "UNKNOWN PANEL";
}
