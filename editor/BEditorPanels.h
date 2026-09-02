#ifndef BASIL_EDITOR_PANELS_H
#define BASIL_EDITOR_PANELS_H

#include "BEditorUIConfig.h"
#include "BProject.h"

#include <filesystem>
#include <string>

void BEditorPanels_DrawScaffolds(
    BEditorUIConfig& config,
    const BProject& project,
    const std::filesystem::path& projectRoot,
    const std::string& editorMessage,
    bool messageIsError
);

#endif
