#ifndef BASIL_EDITOR_THEME_H
#define BASIL_EDITOR_THEME_H

#include "imgui.h"

struct BEditorThemePalette
{
    ImVec4 background;
    ImVec4 surface;
    ImVec4 surfaceRaised;
    ImVec4 cyan;
    ImVec4 violet;
    ImVec4 text;
    ImVec4 textMuted;
    ImVec4 success;
    ImVec4 warning;
    ImVec4 error;
};

// Call between rlImGuiBeginInitImGui() and rlImGuiEndInitImGui().
// Returns false when the bundled font cannot be loaded; the ImGui fallback
// font remains usable and the theme itself is still applied.
bool BEditorTheme_Initialize(float interfaceScale);

// Rebuilds the style from its unscaled baseline before applying scale, so
// repeated calls never compound sizes. This supports the planned scale presets.
void BEditorTheme_Apply(float interfaceScale);

const BEditorThemePalette& BEditorTheme_GetPalette();
ImFont* BEditorTheme_GetRegularFont();
ImFont* BEditorTheme_GetBoldFont();

#endif
