#include "BEditorTheme.h"

#include "raylib.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
constexpr float MINIMUM_SCALE = 1.0f;
constexpr float MAXIMUM_SCALE = 1.75f;
constexpr float BASE_FONT_SIZE = 14.0f;

BEditorThemePalette palette{
    ImVec4(0.025f, 0.035f, 0.055f, 1.0f), // background
    ImVec4(0.045f, 0.060f, 0.085f, 1.0f), // surface
    ImVec4(0.070f, 0.090f, 0.120f, 1.0f), // raised surface
    ImVec4(0.000f, 0.900f, 0.950f, 1.0f), // electric cyan
    ImVec4(0.500f, 0.300f, 0.850f, 1.0f), // restrained violet
    ImVec4(0.850f, 0.925f, 0.950f, 1.0f), // text
    ImVec4(0.480f, 0.590f, 0.650f, 1.0f), // muted text
    ImVec4(0.200f, 0.850f, 0.560f, 1.0f), // success
    ImVec4(0.950f, 0.680f, 0.200f, 1.0f), // warning
    ImVec4(0.950f, 0.250f, 0.300f, 1.0f)  // error
};

ImFont* regularFont = nullptr;
ImFont* boldFont = nullptr;

fs::path FontPath(const char* filename)
{
    return fs::path(GetApplicationDirectory()) / "assets" / "editor" /
        "fonts" / "JetBrainsMono" / filename;
}

void ApplyColors(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = palette.text;
    colors[ImGuiCol_TextDisabled] = palette.textMuted;
    colors[ImGuiCol_WindowBg] = palette.background;
    colors[ImGuiCol_ChildBg] = palette.background;
    colors[ImGuiCol_PopupBg] = palette.surface;
    colors[ImGuiCol_Border] = ImVec4(0.000f, 0.580f, 0.650f, 0.55f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = palette.surface;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.060f, 0.180f, 0.220f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.070f, 0.240f, 0.280f, 1.0f);
    colors[ImGuiCol_TitleBg] = palette.background;
    colors[ImGuiCol_TitleBgActive] = palette.surface;
    colors[ImGuiCol_TitleBgCollapsed] = palette.background;
    colors[ImGuiCol_MenuBarBg] = palette.surface;
    colors[ImGuiCol_ScrollbarBg] = palette.background;
    colors[ImGuiCol_ScrollbarGrab] = palette.surfaceRaised;
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.000f, 0.500f, 0.560f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = palette.cyan;
    colors[ImGuiCol_CheckMark] = palette.cyan;
    colors[ImGuiCol_SliderGrab] = palette.cyan;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.300f, 1.000f, 1.000f, 1.0f);
    colors[ImGuiCol_Button] = palette.surfaceRaised;
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.020f, 0.380f, 0.430f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.000f, 0.580f, 0.640f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.080f, 0.180f, 0.230f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.100f, 0.300f, 0.350f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.100f, 0.390f, 0.430f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(0.000f, 0.480f, 0.540f, 0.65f);
    colors[ImGuiCol_SeparatorHovered] = palette.cyan;
    colors[ImGuiCol_SeparatorActive] = palette.cyan;
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.000f, 0.650f, 0.700f, 0.30f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.000f, 0.800f, 0.850f, 0.75f);
    colors[ImGuiCol_ResizeGripActive] = palette.cyan;
    colors[ImGuiCol_Tab] = palette.surface;
    colors[ImGuiCol_TabHovered] = ImVec4(0.040f, 0.360f, 0.410f, 1.0f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.060f, 0.250f, 0.300f, 1.0f);
    colors[ImGuiCol_TabSelectedOverline] = palette.cyan;
    colors[ImGuiCol_TabDimmed] = palette.background;
    colors[ImGuiCol_TabDimmedSelected] = palette.surface;
    colors[ImGuiCol_PlotLines] = palette.cyan;
    colors[ImGuiCol_PlotHistogram] = palette.violet;
    colors[ImGuiCol_TableHeaderBg] = palette.surfaceRaised;
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.000f, 0.420f, 0.480f, 0.65f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.000f, 0.260f, 0.300f, 0.45f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(palette.violet.x, palette.violet.y, palette.violet.z, 0.45f);
    colors[ImGuiCol_DragDropTarget] = palette.cyan;
    colors[ImGuiCol_NavCursor] = palette.violet;
    colors[ImGuiCol_NavWindowingHighlight] = palette.cyan;
}
}

void BEditorTheme_Apply(float interfaceScale)
{
    interfaceScale = std::clamp(interfaceScale, MINIMUM_SCALE, MAXIMUM_SCALE);

    ImGuiStyle baseline;
    ImGui::GetStyle() = baseline;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    ApplyColors(style);
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.CellPadding = ImVec2(7.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    style.ScrollbarSize = 13.0f;
    style.GrabMinSize = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    style.WindowRounding = 3.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.ScaleAllSizes(interfaceScale);
    style.FontScaleMain = interfaceScale;
    style.MouseCursorScale = interfaceScale;
}

bool BEditorTheme_Initialize(float interfaceScale)
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    BEditorTheme_Apply(interfaceScale);

    float dpiScale = std::max(GetWindowScaleDPI().y, 1.0f);
    ImFontConfig config;
    config.SizePixels = BASE_FONT_SIZE * dpiScale;
    config.PixelSnapH = true;

    fs::path regularPath = FontPath("JetBrainsMono-Regular.ttf");
    fs::path boldPath = FontPath("JetBrainsMono-Bold.ttf");

    if (!fs::is_regular_file(regularPath) || !fs::is_regular_file(boldPath))
        return false;

    regularFont = io.Fonts->AddFontFromFileTTF(regularPath.string().c_str(), config.SizePixels, &config);
    boldFont = io.Fonts->AddFontFromFileTTF(boldPath.string().c_str(), config.SizePixels, &config);

    if (regularFont != nullptr)
        io.FontDefault = regularFont;

    return regularFont != nullptr && boldFont != nullptr;
}

const BEditorThemePalette& BEditorTheme_GetPalette()
{
    return palette;
}

ImFont* BEditorTheme_GetRegularFont()
{
    return regularFont;
}

ImFont* BEditorTheme_GetBoldFont()
{
    return boldFont;
}
