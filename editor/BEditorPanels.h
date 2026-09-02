#ifndef BASIL_EDITOR_PANELS_H
#define BASIL_EDITOR_PANELS_H

#include "BEditorUIConfig.h"
#include "BEditorBuildService.h"
#include "BEditorAssetService.h"
#include "BEditorComponentRegistry.h"
#include "BEditorCodeWorkspace.h"
#include "BEditorTextSpriteDocument.h"
#include "BEditorWorkspaceSession.h"
#include "BProject.h"

#include <filesystem>
#include <string>
#include <utility>

struct BEditorPanelFeedback
{
    BEditorPanelFeedback() = default;
    BEditorPanelFeedback(std::string value, bool error) : message(std::move(value)), isError(error) {}
    std::string message;
    bool isError = false;
    std::string openFile;
    int openLine = 0;
};

BEditorPanelFeedback BEditorPanels_DrawScaffolds(
    BEditorUIConfig& config,
    const BProject& project,
    BEditorWorkspaceSession& workspaceSession,
    BEditorAssetService& assetService,
    const BEditorComponentRegistry& componentRegistry,
    BEditorCodeWorkspace& codeWorkspace,
    BEditorTextSpriteDocument& textSpriteDocument,
    const BEditorBuildService& buildService,
    const std::filesystem::path& projectRoot,
    const std::string& editorMessage,
    bool messageIsError
);

#endif
