#ifndef BASIL_EDITOR_PANELS_H
#define BASIL_EDITOR_PANELS_H

#include "BEditorUIConfig.h"
#include "BEditorBuildService.h"
#include "BEditorWorkspaceSession.h"
#include "BProject.h"

#include <filesystem>
#include <string>

struct BEditorPanelFeedback
{
    std::string message;
    bool isError = false;
};

BEditorPanelFeedback BEditorPanels_DrawScaffolds(
    BEditorUIConfig& config,
    const BProject& project,
    BEditorWorkspaceSession& workspaceSession,
    const BEditorBuildService& buildService,
    const std::filesystem::path& projectRoot,
    const std::string& editorMessage,
    bool messageIsError
);

#endif
