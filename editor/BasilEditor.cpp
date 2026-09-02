#include "BProject.h"
#include "BProjectGenerator.h"
#include "BRecentProjects.h"
#include "BEditorGit.h"
#include "BEditorBuildService.h"
#include "BEditorCodeWorkspace.h"
#include "BEditorComponentRegistry.h"
#include "BEditorPanels.h"
#include "BEditorPlatformDialogs.h"
#include "BEditorPreferences.h"
#include "BEditorTheme.h"
#include "BEditorUIConfig.h"
#include "BEditorWorkspaceSession.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "raylib.h"
#include "rlImGui.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

struct EditorState
{
    BRecentProjects recent{};
    BProject project{};
    fs::path manifestPath;
    fs::path recentPath;
    fs::path preferencesPath;
    fs::path globalUIConfigPath;
    BEditorPreferences preferences{};
    char openPath[BPROJECT_PATH_MAX]{};
    char projectName[BPROJECT_NAME_MAX] = "My Basil Game";
    char identifier[BPROJECT_IDENTIFIER_MAX] = "MyBasilGame";
    char parentDirectory[BPROJECT_PATH_MAX]{};
    int languageMode = static_cast<int>(BPROJECT_LANGUAGE_MIXED);
    int cStandardIndex = 2;
    int cppStandardIndex = 6;
    bool initializeGit = false;
    bool gitInitialized = false;
    double nextGitRefreshTime = 0.0;
    bool projectOpen = false;
    bool resetDockLayout = true;
    BEditorUIConfig uiConfig = BEditorUIConfig_Default();
    BEditorWorkspaceSession workspaceSession;
    BEditorAssetService assetService;
    BEditorComponentRegistry componentRegistry;
    BEditorCodeWorkspace codeWorkspace;
    std::size_t activeCodeTab = 0;
    char codeFind[128]{};
    char codeReplace[128]{};
    std::size_t codeSearchOffset = 0;
    int codeGoToLine = 1;
    int codeCursorTarget = -1;
    double nextCodeRefreshTime = 0.0;
    BEditorTextSpriteDocument textSpriteDocument;
    double nextAssetRefreshTime = 0.0;
    bool confirmProjectClose = false;
    bool confirmApplicationClose = false;
    bool confirmRecovery = false;
    bool showUIConfigManager = false;
    bool exitApproved = false;
    double recoveryDueTime = 0.0;
    std::uint64_t recoveryObservedRevision = 0;
    BEditorBuildService buildService;
    BEditorBuildState observedBuildState = BEditorBuildState::Idle;
    std::string message;
    bool messageIsError = false;
    BTextSpriteCache viewportSpriteCache{};
    BAsciiDrawList viewportDrawList{};
    std::uint64_t viewportRevision = 0;
    std::string viewportError;
    ImVec2 viewportPan{ 0.0f, 0.0f };
    float viewportZoom = 1.0f;
    bool viewportShowMarkers = true;
    bool viewportShowLabels = true;
};

static void ResetViewportPreview(EditorState& state)
{
    BAsciiDrawList_Destroy(&state.viewportDrawList);
    BTextSpriteCache_Destroy(&state.viewportSpriteCache);
    state.viewportRevision = 0;
    state.viewportError.clear();
    state.viewportPan = { 0.0f, 0.0f };
    state.viewportZoom = 1.0f;
}

static bool ApplyAssetMoves(EditorState& state, const std::vector<BEditorAssetMove>& moves, std::string& error)
{
    for (const BEditorAssetMove& move : moves)
    {
        if (!state.workspaceSession.RemapAssetPath(move.oldPath, move.newPath, error)) return false;
        if (state.textSpriteDocument.IsOpen() && state.textSpriteDocument.RelativePath() == move.oldPath)
        {
            if (state.textSpriteDocument.IsDirty())
            { error = "An externally moved Text Sprite has unsaved editor changes."; return false; }
            if (!state.textSpriteDocument.Open(state.manifestPath.parent_path(), move.newPath, error)) return false;
        }
    }
    return true;
}

static fs::path UserHome()
{
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return home != nullptr ? fs::path(home) : fs::current_path();
}

static fs::path DefaultProjectDirectory()
{
    return UserHome() / "Documents" / "BasilEngine";
}

static fs::path EditorDataDirectory()
{
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr)
        return fs::path(appData) / "BasilEngine";
#elif defined(__APPLE__)
    return UserHome() / "Library" / "Application Support" / "BasilEngine";
#else
    const char* configHome = std::getenv("XDG_CONFIG_HOME");
    if (configHome != nullptr)
        return fs::path(configHome) / "BasilEngine";
    return UserHome() / ".config" / "BasilEngine";
#endif
    return UserHome() / ".basilengine";
}

static void ApplyApplicationIcons()
{
    static const int sizes[] = { 16, 24, 32, 48, 64, 128, 256 };
    Image images[IM_ARRAYSIZE(sizes)]{};
    int imageCount = 0;
    fs::path iconRoot = fs::path(GetApplicationDirectory()) / "assets" /
        "editor" / "branding" / "icons";

    for (int size : sizes)
    {
        fs::path path = iconRoot / ("basil-editor-" + std::to_string(size) + ".png");

        if (!fs::is_regular_file(path))
            continue;

        Image image = LoadImage(path.string().c_str());

        if (IsImageValid(image))
            images[imageCount++] = image;
    }

    if (imageCount > 0)
        SetWindowIcons(images, imageCount);

    for (int i = 0; i < imageCount; ++i)
        UnloadImage(images[i]);
}

static void SetMessage(EditorState& state, const char* message, bool isError)
{
    state.message = message != nullptr ? message : "Unknown error.";
    state.messageIsError = isError;
}

static void SaveRecentProjects(EditorState& state)
{
    BProjectError error{};
    std::error_code directoryError;
    fs::create_directories(state.recentPath.parent_path(), directoryError);

    if (directoryError || !BRecentProjects_Save(state.recentPath.string().c_str(), &state.recent, &error))
        SetMessage(state, directoryError ? directoryError.message().c_str() : error.message, true);
}

static bool OpenProject(EditorState& state, const fs::path& manifestPath)
{
    ResetViewportPreview(state);
    BProjectError error{};
    std::error_code pathError;
    fs::path absolutePath = fs::absolute(manifestPath, pathError).lexically_normal();

    if (pathError || !BProject_Load(absolutePath.string().c_str(), &state.project, &error))
    {
        SetMessage(state, pathError ? pathError.message().c_str() : error.message, true);
        return false;
    }

    state.manifestPath = absolutePath;
    state.gitInitialized = BEditorGit_IsInitialized(absolutePath.parent_path());
    state.nextGitRefreshTime = GetTime() + 1.0;
    state.projectOpen = true;
    state.resetDockLayout = true;
    state.uiConfig = BEditorUIConfig_Default();
    std::string uiConfigError;
    fs::path projectUIConfig = absolutePath.parent_path() / ".basil" / "editor-ui.basilui.json";
    if (fs::is_regular_file(projectUIConfig))
        BEditorUIConfig_Load(projectUIConfig.string(), state.uiConfig, uiConfigError);
    else if (fs::is_regular_file(state.globalUIConfigPath))
        BEditorUIConfig_Load(state.globalUIConfigPath.string(), state.uiConfig, uiConfigError);
    state.workspaceSession.Reset();
    std::string workspaceError;
    bool workspaceLoaded = state.workspaceSession.Load(
        absolutePath.parent_path(),
        state.project.startupWorkspace,
        workspaceError
    );
    std::string assetError;
    bool assetsOpened = state.assetService.Open(absolutePath.parent_path(), assetError);
    std::string componentError;
    bool componentsOpened = state.componentRegistry.Open(absolutePath.parent_path(), componentError);
    std::string codeError;
    bool codeOpened = state.codeWorkspace.OpenProject(absolutePath.parent_path(), codeError);
    state.nextAssetRefreshTime = GetTime() + 1.0;

    if (!workspaceLoaded || !componentsOpened || !codeOpened)
        state.uiConfig.showProblems = true;
    else
        state.confirmRecovery = state.workspaceSession.HasNewerRecovery();
    state.recoveryObservedRevision = state.workspaceSession.Revision();
    state.recoveryDueTime = 0.0;

    BRecentProjects_Add(&state.recent, absolutePath.string().c_str());
    SaveRecentProjects(state);
    SetWindowTitle(TextFormat("BasilEditor - %s", state.project.name));
    SetMessage(
        state,
        !uiConfigError.empty() ? uiConfigError.c_str() : !assetsOpened ? assetError.c_str() : !componentsOpened ? componentError.c_str() : !codeOpened ? codeError.c_str() : workspaceLoaded ?
            (state.workspaceSession.RequiresMigration() ?
                "Project opened. Startup Workspace will migrate safely on first save." :
                "Project and startup Workspace opened successfully.") :
            workspaceError.c_str(),
        !uiConfigError.empty() || !assetsOpened || !componentsOpened || !codeOpened || !workspaceLoaded
    );
    return true;
}

static void CreateProject(EditorState& state)
{
    BProject project = BProject_Default(state.projectName, state.identifier);
    static const int cStandards[] = { 90, 99, 11, 17, 23 };
    static const int cppStandards[] = { 98, 11, 14, 17, 20, 23, 26 };
    project.languageMode = static_cast<BProjectLanguageMode>(state.languageMode);
    project.cStandard = cStandards[state.cStandardIndex];
    project.cppStandard = cppStandards[state.cppStandardIndex];

    std::error_code directoryError;
    fs::create_directories(state.parentDirectory, directoryError);

    if (directoryError)
    {
        SetMessage(state, directoryError.message().c_str(), true);
        return;
    }

    BProjectError error{};

    if (!BProjectGenerator_Create(&project, state.parentDirectory, &error))
    {
        SetMessage(state, error.message, true);
        return;
    }

    fs::path root = fs::path(state.parentDirectory) / project.identifier;

    bool gitFailed = state.initializeGit && !BEditorGit_Initialize(root);

    if (OpenProject(state, root / (std::string(project.identifier) + ".basilproject")) && gitFailed)
        SetMessage(state, "The project was created, but Git initialization failed. You can retry it from the project overview.", true);
}

static void DrawMessage(const EditorState& state)
{
    if (state.message.empty())
        return;

    const BEditorThemePalette& palette = BEditorTheme_GetPalette();
    ImVec4 color = state.messageIsError ? palette.error : palette.success;
    ImVec4 background = color;
    background.w = 0.10f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(color.x, color.y, color.z, 0.60f));
    ImGui::BeginChild("##StatusMessage", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 2.2f), true);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(state.messageIsError ? "[!] SYSTEM NOTICE" : "[+] STATUS");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", state.message.c_str());
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

static void DrawInterfaceScale(EditorState& state)
{
    static const char* labels[] = { "100%", "115%", "135%", "150%", "175%" };
    static const float scales[] = { 1.0f, 1.15f, 1.35f, 1.5f, 1.75f };
    int selected = 2;

    for (int i = 0; i < IM_ARRAYSIZE(scales); ++i)
    {
        if (state.preferences.interfaceScale == scales[i])
        {
            selected = i;
            break;
        }
    }

    ImGui::SetNextItemWidth(100.0f * state.preferences.interfaceScale);

    if (!ImGui::Combo("Interface scale", &selected, labels, IM_ARRAYSIZE(labels)))
        return;

    state.preferences.interfaceScale = scales[selected];
    BEditorTheme_Apply(state.preferences.interfaceScale);
    std::string error;

    if (!BEditorPreferences_Save(state.preferencesPath.string(), state.preferences, error))
        SetMessage(state, error.c_str(), true);
    else
        SetMessage(state, "Interface scale saved.", false);
}

static void DrawHeading(const char* text)
{
    ImFont* bold = BEditorTheme_GetBoldFont();

    if (bold != nullptr)
        ImGui::PushFont(bold);

    ImGui::TextUnformatted(text);

    if (bold != nullptr)
        ImGui::PopFont();
}

static void DrawBrandRail(EditorState& state)
{
    const BEditorThemePalette& palette = BEditorTheme_GetPalette();
    float width = 250.0f * state.preferences.interfaceScale;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, palette.surface);
    ImGui::BeginChild("##BrandRail", ImVec2(width, 0.0f), true);
    ImGui::TextColored(palette.cyan, "[ BASIL//EDITOR ]");
    ImGui::Spacing();
    DrawHeading("BASILENGINE");
    ImGui::TextColored(palette.violet, "ASCII SYSTEMS WORKBENCH");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("PROJECT LINK");
    ImGui::TextColored(palette.success, "● ONLINE");
    ImGui::Spacing();
    ImGui::TextDisabled("RUNTIME TARGET");
    ImGui::TextUnformatted("C11 // C++26");
    ImGui::Spacing();
    ImGui::TextDisabled("PROJECT ROOT");
    ImGui::TextWrapped("%s", state.parentDirectory);
    ImGui::Dummy(ImVec2(0.0f, 18.0f * state.preferences.interfaceScale));
    ImGui::SeparatorText("DISPLAY");
    DrawInterfaceScale(state);

    float remaining = ImGui::GetContentRegionAvail().y;
    if (remaining > ImGui::GetTextLineHeightWithSpacing() * 3.0f)
        ImGui::Dummy(ImVec2(0.0f, remaining - ImGui::GetTextLineHeightWithSpacing() * 2.0f));

    ImGui::TextDisabled("BUILD CHANNEL // DEV");
    ImGui::TextColored(palette.cyan, "> awaiting operator input_");
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void DrawRecentProjects(EditorState& state)
{
    if (state.recent.count == 0)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("NO PROJECT LINKS RECORDED");
        ImGui::TextWrapped("Create a new Project or open an existing .basilproject manifest to establish a recent link.");
        return;
    }

    bool removed = false;

    for (size_t i = 0; i < state.recent.count && !removed; ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        fs::path path(state.recent.paths[i]);
        ImGui::BeginChild("##RecentProject", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 4.6f), true);
        DrawHeading(path.stem().string().c_str());
        ImGui::TextDisabled("%s", state.recent.paths[i]);
        ImGui::Spacing();

        if (ImGui::Button("OPEN LINK", ImVec2(150.0f * state.preferences.interfaceScale, 0.0f)))
            OpenProject(state, path);

        ImGui::SameLine();

        if (ImGui::Button("REMOVE"))
        {
            BRecentProjects_Remove(&state.recent, i);
            SaveRecentProjects(state);
            removed = true;
        }

        ImGui::EndChild();
        ImGui::Spacing();
        ImGui::PopID();
    }
}

static void DrawOpenProject(EditorState& state)
{
    ImGui::TextDisabled("MANIFEST ENDPOINT");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##ManifestPath", state.openPath, sizeof(state.openPath));
    ImGui::Spacing();

    if (ImGui::Button("BROWSE...", ImVec2(-1.0f, 0.0f)))
    {
        fs::path selected = state.openPath;
        std::string error;
        if (BEditorDialog_OpenProject(selected, error))
            std::snprintf(state.openPath, sizeof(state.openPath), "%s", selected.string().c_str());
        else if (!error.empty())
            SetMessage(state, error.c_str(), true);
    }

    if (ImGui::Button("OPEN PROJECT LINK", ImVec2(-1.0f, 0.0f)))
        OpenProject(state, state.openPath);
}

static void DrawNewProject(EditorState& state)
{
    const char* modes[] = { "C only", "C++ only", "C and C++" };
    const char* cStandards[] = { "C90", "C99", "C11", "C17", "C23" };
    const char* cppStandards[] = { "C++98", "C++11", "C++14", "C++17", "C++20", "C++23", "C++26" };

    ImGui::TextDisabled("PROJECT IDENTITY");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("Display name", state.projectName, sizeof(state.projectName));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("Code identifier", state.identifier, sizeof(state.identifier));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("Project location", state.parentDirectory, sizeof(state.parentDirectory));
    if (ImGui::Button("SELECT PROJECT LOCATION...", ImVec2(-1.0f, 0.0f)))
    {
        fs::path selected = state.parentDirectory;
        std::string error;
        if (BEditorDialog_SelectFolder(selected, error))
            std::snprintf(state.parentDirectory, sizeof(state.parentDirectory), "%s", selected.string().c_str());
        else if (!error.empty())
            SetMessage(state, error.c_str(), true);
    }
    ImGui::Spacing();
    ImGui::SeparatorText("LANGUAGE MATRIX");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("Languages", &state.languageMode, modes, IM_ARRAYSIZE(modes));

    if (state.languageMode != static_cast<int>(BPROJECT_LANGUAGE_CPP))
    {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("C standard", &state.cStandardIndex, cStandards, IM_ARRAYSIZE(cStandards));
    }

    if (state.languageMode != static_cast<int>(BPROJECT_LANGUAGE_C))
    {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("C++ standard", &state.cppStandardIndex, cppStandards, IM_ARRAYSIZE(cppStandards));
    }

    ImGui::Checkbox("Initialize a Git repository", &state.initializeGit);
    ImGui::Spacing();

    if (ImGui::Button("GENERATE + OPEN PROJECT", ImVec2(-1.0f, 0.0f)))
        CreateProject(state);
}

static void DrawProjectBrowser(EditorState& state)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("BasilEngine Project Browser", nullptr, ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    DrawBrandRail(state);
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("##ProjectBrowserContent", ImVec2(0.0f, 0.0f), false);
    ImGui::Dummy(ImVec2(0.0f, 12.0f * state.preferences.interfaceScale));
    ImGui::TextDisabled("BASIL NETWORK // PROJECT ACCESS");
    DrawHeading("PROJECT BROWSER");
    ImGui::TextDisabled("Create a clean Project or reconnect to an existing manifest.");
    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::BeginTabBar("ProjectActions"))
    {
        if (ImGui::BeginTabItem("RECENT LINKS"))
        {
            ImGui::Spacing();
            DrawRecentProjects(state);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("OPEN PROJECT"))
        {
            ImGui::Spacing();
            DrawOpenProject(state);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("NEW PROJECT"))
        {
            ImGui::Spacing();
            DrawNewProject(state);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Spacing();
    DrawMessage(state);
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
}

static void ReturnToProjectBrowser(EditorState& state)
{
    if (state.buildService.IsBusy())
    {
        SetMessage(state, "Stop the active build or game before closing the Project.", true);
        return;
    }

    if (state.workspaceSession.IsDirty() || state.textSpriteDocument.IsDirty() || state.codeWorkspace.HasDirtyDocuments())
    {
        state.confirmProjectClose = true;
        return;
    }

    state.projectOpen = false;
    state.workspaceSession.Reset();
    state.assetService.Reset();
    state.textSpriteDocument.Reset();
    state.codeWorkspace = BEditorCodeWorkspace{};
    ResetViewportPreview(state);
    SetWindowTitle("BasilEditor");
}

static bool SaveWorkspace(EditorState& state)
{
    std::string error;
    if (!state.codeWorkspace.SaveAll(error))
    {
        SetMessage(state, error.c_str(), true);
        return false;
    }
    if (state.textSpriteDocument.IsDirty() && !state.textSpriteDocument.Save(error))
    {
        SetMessage(state, error.c_str(), true);
        return false;
    }
    bool succeeded = state.workspaceSession.Save(error);
    SetMessage(state, succeeded ? "All modified authoring documents saved." : error.c_str(), !succeeded);
    return succeeded;
}

static void StartProjectBuild(EditorState& state, bool runAfterBuild)
{
    if (runAfterBuild)
    {
        BDiagnosticList diagnostics{};
        std::string validationError;
        if (!state.workspaceSession.ValidateForRun(diagnostics, validationError))
        {
            std::vector<std::string> problems;
            for (std::size_t i = 0; i < diagnostics.count; ++i)
            {
                const BDiagnostic& diagnostic = diagnostics.items[i];
                if (diagnostic.severity != BDIAGNOSTIC_ERROR)
                    continue;
                std::string problem = diagnostic.path[0] ? std::string(diagnostic.path) : "Workspace";
                if (diagnostic.line > 0)
                    problem += ":" + std::to_string(diagnostic.line) + ":" + std::to_string(diagnostic.column);
                if (diagnostic.entityId[0])
                    problem += " [" + std::string(diagnostic.entityId) + "]";
                if (diagnostic.componentType[0])
                    problem += " [" + std::string(diagnostic.componentType) + "]";
                problem += ": " + std::string(diagnostic.message);
                problems.push_back(std::move(problem));
            }
            state.buildService.ReportPreflightFailure(problems);
            SetMessage(state, validationError.c_str(), true);
            return;
        }
    }

    if ((state.workspaceSession.IsDirty() || state.textSpriteDocument.IsDirty() || state.codeWorkspace.HasDirtyDocuments()) && !SaveWorkspace(state))
        return;

    std::string error;
    bool started = state.buildService.StartBuild(
        state.manifestPath.parent_path(),
        state.manifestPath,
        state.project,
        runAfterBuild,
        error
    );

    if (!started)
    {
        SetMessage(state, error.c_str(), true);
        return;
    }

    state.uiConfig.showBuildOutput = true;
    state.observedBuildState = state.buildService.State();
    SetMessage(state, runAfterBuild ? "Project build started; the game will run after success." : "Project build started.", false);
}

static void CaptureUIConfigLayout(EditorState& state)
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (display.x <= 0.0f || display.y <= 0.0f) return;
    ImGuiWindow* left = ImGui::FindWindowByName(BEditorPanel_Name(BEditorPanel::ProjectDetails));
    ImGuiWindow* right = ImGui::FindWindowByName(BEditorPanel_Name(BEditorPanel::Inspector));
    ImGuiWindow* bottom = ImGui::FindWindowByName(BEditorPanel_Name(BEditorPanel::Assets));
    if (left && left->DockNode) state.uiConfig.leftRatio = std::clamp(left->DockNode->Size.x / display.x, 0.1f, 0.4f);
    if (right && right->DockNode) state.uiConfig.rightRatio = std::clamp(right->DockNode->Size.x / display.x, 0.1f, 0.4f);
    if (bottom && bottom->DockNode) state.uiConfig.bottomRatio = std::clamp(bottom->DockNode->Size.y / display.y, 0.1f, 0.45f);
}

static void DrawEditorMenuBar(EditorState& state)
{
    const BEditorThemePalette& palette = BEditorTheme_GetPalette();

    if (!ImGui::BeginMainMenuBar())
        return;

    ImGui::TextColored(palette.cyan, "[ BASIL//EDITOR ]");
    ImGui::Separator();

    if (ImGui::BeginMenu("Project"))
    {
        ImGui::TextDisabled("ACTIVE // %s", state.project.name);
        ImGui::Separator();

        if (ImGui::MenuItem("Return to Project Browser"))
            ReturnToProjectBrowser(state);

        ImGui::Separator();
        if (ImGui::MenuItem("Exit BasilEditor"))
        {
            state.confirmApplicationClose = state.workspaceSession.IsDirty() || state.textSpriteDocument.IsDirty() || state.codeWorkspace.HasDirtyDocuments();
            state.exitApproved = !state.confirmApplicationClose;
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Workspace"))
    {
        ImGui::BeginDisabled();
        ImGui::MenuItem("New Workspace");
        ImGui::MenuItem("Open Workspace...");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!state.workspaceSession.IsLoaded());

        ImGui::BeginDisabled(!state.workspaceSession.CanUndo());
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) { std::string error; if (!state.workspaceSession.Undo(error)) SetMessage(state, error.c_str(), true); }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!state.workspaceSession.CanRedo());
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) { std::string error; if (!state.workspaceSession.Redo(error)) SetMessage(state, error.c_str(), true); }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(state.workspaceSession.SelectedEntity() == nullptr);
        if (ImGui::MenuItem("Duplicate Entity", "Ctrl+D")) { std::string error; bool ok = state.workspaceSession.DuplicateSelectedEntity(error); SetMessage(state, ok ? "Entity duplicated." : error.c_str(), !ok); }
        ImGui::EndDisabled();
        ImGui::Separator();

        if (ImGui::MenuItem("Save All", "Ctrl+S"))
            SaveWorkspace(state);

        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Project Details", nullptr, &state.uiConfig.showProjectDetails);
        ImGui::MenuItem("Workspace Hierarchy", nullptr, &state.uiConfig.showWorkspaceHierarchy);
        ImGui::MenuItem("Inspector", nullptr, &state.uiConfig.showInspector);
        ImGui::MenuItem("Workspace Viewport", nullptr, &state.uiConfig.showWorkspaceViewport);
        ImGui::MenuItem("Assets", nullptr, &state.uiConfig.showAssets);
        ImGui::MenuItem("Console", nullptr, &state.uiConfig.showConsole);
        ImGui::MenuItem("Build Output", nullptr, &state.uiConfig.showBuildOutput);
        ImGui::MenuItem("Problems", nullptr, &state.uiConfig.showProblems);
        ImGui::MenuItem("Terminal", nullptr, &state.uiConfig.showTerminal);
        ImGui::MenuItem("Text Sprite Editor", nullptr, &state.uiConfig.showTextSpriteEditor);
        ImGui::MenuItem("Code Editor", nullptr, &state.uiConfig.showCodeEditor);
        ImGui::Separator();

        ImGui::MenuItem("UI Config Manager", nullptr, &state.showUIConfigManager);

        if (ImGui::MenuItem("Reset Default UI Config"))
        {
            state.uiConfig = BEditorUIConfig_Default();
            state.resetDockLayout = true;
            SetMessage(state, "Default UI Config restored.", false);
        }

        if (ImGui::MenuItem("Save as Global Default"))
        {
            CaptureUIConfigLayout(state);
            std::string error; bool ok = BEditorUIConfig_Save(state.globalUIConfigPath.string(), state.uiConfig, error);
            SetMessage(state, ok ? "Global UI Config saved." : error.c_str(), !ok);
        }
        if (ImGui::MenuItem("Save for This Project"))
        {
            CaptureUIConfigLayout(state);
            fs::path path = state.manifestPath.parent_path() / ".basil" / "editor-ui.basilui.json";
            std::string error; bool ok = BEditorUIConfig_Save(path.string(), state.uiConfig, error);
            SetMessage(state, ok ? "Project UI Config saved." : error.c_str(), !ok);
        }
        if (ImGui::MenuItem("Import UI Config..."))
        {
            fs::path path; std::string error;
            if (BEditorDialog_OpenUIConfig(path, error))
            {
                BEditorUIConfig loaded;
                bool ok = BEditorUIConfig_Load(path.string(), loaded, error);
                if (ok) { state.uiConfig = loaded; state.resetDockLayout = true; }
                SetMessage(state, ok ? "UI Config imported." : error.c_str(), !ok);
            }
            else if (!error.empty()) SetMessage(state, error.c_str(), true);
        }
        if (ImGui::MenuItem("Export UI Config..."))
        {
            fs::path path = "BasilEditor.basilui.json"; std::string error;
            if (BEditorDialog_SaveUIConfig(path, error))
            {
                CaptureUIConfigLayout(state);
                bool ok = BEditorUIConfig_Save(path.string(), state.uiConfig, error);
                SetMessage(state, ok ? "UI Config exported." : error.c_str(), !ok);
            }
            else if (!error.empty()) SetMessage(state, error.c_str(), true);
        }

        ImGui::EndMenu();
    }

    ImGui::BeginDisabled(state.buildService.IsBusy());

    if (ImGui::MenuItem("Build"))
        StartProjectBuild(state, false);

    if (ImGui::MenuItem("Run"))
        StartProjectBuild(state, true);

    ImGui::EndDisabled();

    ImGui::BeginDisabled(!state.buildService.IsGameActive());

    if (state.buildService.State() == BEditorBuildState::Paused)
    {
        if (ImGui::MenuItem("Resume"))
        {
            std::string error;
            bool succeeded = state.buildService.Resume(error);
            SetMessage(state, succeeded ? "Game resumed." : error.c_str(), !succeeded);
        }
    }
    else if (ImGui::MenuItem("Pause"))
    {
        std::string error;
        bool succeeded = state.buildService.Pause(error);
        SetMessage(state, succeeded ? "Game paused." : error.c_str(), !succeeded);
    }

    ImGui::EndDisabled();
    ImGui::BeginDisabled(!state.buildService.IsBusy());

    if (ImGui::MenuItem("Stop"))
    {
        std::string error;
        bool succeeded = state.buildService.Stop(error);
        SetMessage(state, succeeded ? "Active process stopped." : error.c_str(), !succeeded);
    }

    ImGui::EndDisabled();

    if (ImGui::MenuItem("Terminal"))
        state.uiConfig.showTerminal = true;

    const char* statusText = (state.workspaceSession.IsDirty() || state.textSpriteDocument.IsDirty() || state.codeWorkspace.HasDirtyDocuments()) ?
        "WORKSPACE // MODIFIED" : "PROJECT LINK // STABLE";
    float statusWidth = ImGui::CalcTextSize(statusText).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - statusWidth - ImGui::GetStyle().ItemSpacing.x);
    ImGui::TextColored(
        (state.workspaceSession.IsDirty() || state.textSpriteDocument.IsDirty() || state.codeWorkspace.HasDirtyDocuments()) ? palette.warning : palette.success,
        statusText
    );
    ImGui::EndMainMenuBar();
}

static void BuildDefaultDockLayout(ImGuiID dockspaceId, const BEditorUIConfig& config)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

    ImGuiID center = dockspaceId;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, config.leftRatio, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, config.rightRatio, nullptr, &center);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, config.bottomRatio, nullptr, &center);

    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::ProjectDetails), left);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::WorkspaceHierarchy), left);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::Inspector), right);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::CodeEditor), center);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::WorkspaceViewport), center);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::Assets), bottom);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::Console), bottom);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::BuildOutput), bottom);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::Problems), bottom);
    ImGui::DockBuilderDockWindow(BEditorPanel_Name(BEditorPanel::Terminal), bottom);
    ImGui::DockBuilderFinish(dockspaceId);
}

static void DrawProjectDetails(EditorState& state)
{
    if (!state.uiConfig.showProjectDetails)
        return;

    ImGui::Begin(
        BEditorPanel_Name(BEditorPanel::ProjectDetails),
        &state.uiConfig.showProjectDetails
    );
    const BEditorThemePalette& palette = BEditorTheme_GetPalette();
    ImGui::TextColored(palette.cyan, "[ PROJECT//ACTIVE ]");
    DrawHeading(state.project.name);
    ImGui::Separator();
    ImGui::TextDisabled("PROJECT MANIFEST");
    ImGui::TextWrapped("%s", state.manifestPath.string().c_str());
    ImGui::Spacing();
    ImGui::SeparatorText("IDENTITY");
    ImGui::TextDisabled("CODE IDENTIFIER");
    ImGui::TextUnformatted(state.project.identifier);
    ImGui::TextDisabled("STARTUP WORKSPACE");
    ImGui::TextWrapped("%s", state.project.startupWorkspace);
    ImGui::TextDisabled("LANGUAGE MODE");
    ImGui::TextUnformatted(BProject_LanguageModeToString(state.project.languageMode));
    ImGui::TextDisabled("LANGUAGE STANDARDS");
    ImGui::Text("C%d // C++%d", state.project.cStandard, state.project.cppStandard);
    ImGui::Spacing();
    ImGui::SeparatorText("PROJECT OPERATIONS");

    ImGui::BeginDisabled(state.gitInitialized);
    const char* gitButtonLabel = state.gitInitialized ?
        "GIT REPOSITORY ACTIVE" : "INITIALIZE GIT";

    if (ImGui::Button(gitButtonLabel, ImVec2(-1.0f, 0.0f)))
    {
        bool succeeded = BEditorGit_Initialize(state.manifestPath.parent_path());
        state.gitInitialized = succeeded;
        SetMessage(state, succeeded ? "Git repository initialized." : "Git initialization failed. Verify that Git is installed and available on PATH.", !succeeded);
    }

    ImGui::EndDisabled();
    ImGui::Spacing();
    DrawInterfaceScale(state);
    ImGui::Spacing();
    DrawMessage(state);
    ImGui::End();
}

static void DrawWorkspaceViewport(EditorState& state)
{
    if (!state.uiConfig.showWorkspaceViewport)
        return;

    const BEditorThemePalette& palette = BEditorTheme_GetPalette();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette.background);
    ImGui::Begin(
        BEditorPanel_Name(BEditorPanel::WorkspaceViewport),
        &state.uiConfig.showWorkspaceViewport
    );
    ImGui::TextColored(palette.violet, "WORKSPACE VIEWPORT // AUTHORING PREVIEW");
    ImGui::SameLine();
    ImGui::TextDisabled("NOT PLAY MODE");
    ImGui::Separator();

    if (!state.workspaceSession.IsLoaded())
    {
        ImGui::TextColored(palette.error, "[ STARTUP WORKSPACE UNAVAILABLE ]");
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }

    if (ImGui::Button("RESET VIEW"))
    {
        state.viewportPan = { 0.0f, 0.0f };
        state.viewportZoom = 1.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("REFRESH ASSETS"))
        state.viewportRevision = 0;
    ImGui::SameLine();
    ImGui::Checkbox("Markers", &state.viewportShowMarkers);
    ImGui::SameLine();
    ImGui::Checkbox("Labels", &state.viewportShowLabels);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    ImGui::SliderFloat("Zoom", &state.viewportZoom, 0.35f, 3.0f, "%.2fx");

    std::uint64_t revision = state.workspaceSession.Revision();
    if (state.viewportRevision != revision)
    {
        BDiagnosticList diagnostics{};
        if (BAsciiDrawList_Build(
                &state.workspaceSession.Workspace(),
                state.manifestPath.parent_path().string().c_str(),
                &state.viewportSpriteCache,
                &state.viewportDrawList,
                &diagnostics))
        {
            state.viewportError.clear();
        }
        else
        {
            const BDiagnostic* error = BDiagnosticList_FirstError(&diagnostics);
            state.viewportError = error != nullptr ? error->message : "Viewport preview could not be built.";
        }
        state.viewportRevision = revision;
    }

    if (!state.viewportError.empty())
        ImGui::TextColored(palette.error, "[ PREVIEW ERROR ] %s", state.viewportError.c_str());

    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = std::max(canvasSize.x, 64.0f);
    canvasSize.y = std::max(canvasSize.y, 64.0f);
    ImVec2 canvasStart = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##WorkspaceViewportCanvas", canvasSize, ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        state.viewportPan.x += delta.x;
        state.viewportPan.y += delta.y;
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
        state.viewportZoom = std::clamp(state.viewportZoom + ImGui::GetIO().MouseWheel * 0.1f, 0.35f, 3.0f);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 canvasEnd{ canvasStart.x + canvasSize.x, canvasStart.y + canvasSize.y };
    draw->AddRectFilled(canvasStart, canvasEnd, ImGui::ColorConvertFloat4ToU32(palette.background));
    draw->PushClipRect(canvasStart, canvasEnd, true);
    float fontSize = ImGui::GetFontSize() * state.viewportZoom;
    float cellWidth = ImGui::GetFontSize() * 0.68f * state.viewportZoom;
    float cellHeight = ImGui::GetFontSize() * 1.25f * state.viewportZoom;
    ImVec2 origin{
        canvasStart.x + canvasSize.x * 0.5f + state.viewportPan.x,
        canvasStart.y + canvasSize.y * 0.5f + state.viewportPan.y
    };
    ImU32 gridColor = ImGui::ColorConvertFloat4ToU32(ImVec4(palette.cyan.x, palette.cyan.y, palette.cyan.z, 0.08f));
    for (float x = origin.x; x < canvasEnd.x; x += cellWidth) draw->AddLine({ x, canvasStart.y }, { x, canvasEnd.y }, gridColor);
    for (float x = origin.x - cellWidth; x > canvasStart.x; x -= cellWidth) draw->AddLine({ x, canvasStart.y }, { x, canvasEnd.y }, gridColor);
    for (float y = origin.y; y < canvasEnd.y; y += cellHeight) draw->AddLine({ canvasStart.x, y }, { canvasEnd.x, y }, gridColor);
    for (float y = origin.y - cellHeight; y > canvasStart.y; y -= cellHeight) draw->AddLine({ canvasStart.x, y }, { canvasEnd.x, y }, gridColor);
    draw->AddLine({ canvasStart.x, origin.y }, { canvasEnd.x, origin.y }, ImGui::ColorConvertFloat4ToU32(ImVec4(palette.cyan.x, palette.cyan.y, palette.cyan.z, 0.35f)), 1.5f);
    draw->AddLine({ origin.x, canvasStart.y }, { origin.x, canvasEnd.y }, ImGui::ColorConvertFloat4ToU32(ImVec4(palette.cyan.x, palette.cyan.y, palette.cyan.z, 0.35f)), 1.5f);

    const BWorkspaceEntity* selected = state.workspaceSession.SelectedEntity();
    for (std::size_t i = 0; i < state.viewportDrawList.count; ++i)
    {
        const BAsciiDrawItem& item = state.viewportDrawList.items[i];
        ImVec2 position{ origin.x + item.x * cellWidth, origin.y + item.y * cellHeight };
        ImU32 background = IM_COL32(item.background.r, item.background.g, item.background.b, item.background.a);
        if (item.background.a > 0)
            draw->AddRectFilled(position, { position.x + cellWidth, position.y + cellHeight }, background);
        char glyph[2] = { item.glyph, '\0' };
        draw->AddText(BEditorTheme_GetRegularFont(), fontSize, position,
            IM_COL32(item.foreground.r, item.foreground.g, item.foreground.b, item.foreground.a), glyph);
        if (selected != nullptr && std::strcmp(selected->id, item.entityId) == 0)
            draw->AddRect(position, { position.x + cellWidth, position.y + cellHeight }, ImGui::ColorConvertFloat4ToU32(palette.violet));
    }

    if (state.viewportShowMarkers)
    {
        const BWorkspaceDocument& workspace = state.workspaceSession.Workspace();
        for (std::size_t i = 0; i < workspace.entityCount; ++i)
        {
            const BWorkspaceEntity& entity = workspace.entities[i];
            const BWorkspaceComponent* transform = BWorkspaceEntity_FindComponentConst(&entity, BWORKSPACE_TRANSFORM2D_TYPE);
            const BWorkspaceComponent* renderable = BWorkspaceEntity_FindComponentConst(&entity, BWORKSPACE_ASCII_RENDERABLE_TYPE);
            bool activeRenderable = renderable != nullptr && renderable->data.asciiRenderable.visible;
            if (!entity.enabled || transform == nullptr || activeRenderable)
                continue;
            ImVec2 marker{
                origin.x + transform->data.transform2d.x * cellWidth,
                origin.y + transform->data.transform2d.y * cellHeight
            };
            bool isSelected = selected != nullptr && std::strcmp(selected->id, entity.id) == 0;
            ImU32 color = ImGui::ColorConvertFloat4ToU32(isSelected ? palette.violet : palette.cyan);
            float radius = 6.0f * state.viewportZoom;
            draw->AddLine({ marker.x - radius, marker.y }, { marker.x + radius, marker.y }, color, 2.0f);
            draw->AddLine({ marker.x, marker.y - radius }, { marker.x, marker.y + radius }, color, 2.0f);
            if (state.viewportShowLabels)
                draw->AddText({ marker.x + radius + 3.0f, marker.y - fontSize * 0.5f }, color, entity.name);
        }
    }
    draw->PopClipRect();
    ImGui::End();
    ImGui::PopStyleColor();
}

static void DrawCodeEditor(EditorState& state)
{
    if (!state.uiConfig.showCodeEditor) return;
    if (!ImGui::Begin(BEditorPanel_Name(BEditorPanel::CodeEditor), &state.uiConfig.showCodeEditor)) { ImGui::End(); return; }
    ImGui::BeginChild("##ProjectFiles", ImVec2(240.0f, 0.0f), true);
    ImGui::TextDisabled("PROJECT FILES");
    static char newFile[256] = "source/new_file.c";
    if (ImGui::SmallButton("+ FILE")) ImGui::OpenPopup("CREATE PROJECT FILE");
    if (ImGui::BeginPopup("CREATE PROJECT FILE"))
    {
        ImGui::InputText("Project path", newFile, sizeof(newFile));
        if (ImGui::Button("CREATE"))
        {
            std::string error; bool ok = state.codeWorkspace.CreateFile(newFile, error);
            SetMessage(state, ok ? "Project file created." : error.c_str(), !ok);
            if (ok) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("REFRESH")) { std::string error; if (!state.codeWorkspace.RefreshTree(error)) SetMessage(state, error.c_str(), true); }
    ImGui::Separator();
    for (const std::string& path : state.codeWorkspace.Files())
    {
        if (ImGui::Selectable(path.c_str(), false))
        {
            std::string error;
            if (!state.codeWorkspace.OpenFile(path, error)) SetMessage(state, error.c_str(), true);
            else
            {
                const auto& documents = state.codeWorkspace.Documents();
                for (std::size_t i = 0; i < documents.size(); ++i) if (documents[i].relativePath == path) state.activeCodeTab = i;
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginGroup();
    auto& documents = state.codeWorkspace.Documents();
    if (documents.empty())
        ImGui::TextDisabled("Select a C, C++, CMake, JSON, or text file from the Project tree.");
    else
    {
        if (state.activeCodeTab >= documents.size()) state.activeCodeTab = documents.size() - 1;
        if (ImGui::BeginTabBar("##CodeTabs"))
        {
            for (std::size_t i = 0; i < documents.size(); ++i)
            {
                std::string label = documents[i].relativePath + (documents[i].dirty ? " *" : "");
                if (ImGui::BeginTabItem(label.c_str(), nullptr, i == state.activeCodeTab ? ImGuiTabItemFlags_SetSelected : 0))
                { state.activeCodeTab = i; ImGui::EndTabItem(); }
            }
            ImGui::EndTabBar();
        }
        BEditorCodeDocument& document = documents[state.activeCodeTab];
        if (ImGui::SmallButton("SAVE")) { std::string error; bool ok = state.codeWorkspace.Save(state.activeCodeTab, error); SetMessage(state, ok ? "Source file saved." : error.c_str(), !ok); }
        ImGui::SameLine();
        if (ImGui::SmallButton("EXTERNAL EDITOR")) { std::string error; bool ok = BEditorPlatform_OpenExternalEditor(state.codeWorkspace.Root() / document.relativePath, error); SetMessage(state, ok ? "Opened external editor." : error.c_str(), !ok); }
        ImGui::SameLine();
        if (ImGui::SmallButton("REVEAL")) { std::string error; bool ok = BEditorPlatform_RevealFile(state.codeWorkspace.Root() / document.relativePath, error); SetMessage(state, ok ? "Revealed Project file." : error.c_str(), !ok); }
        ImGui::SameLine();
        if (ImGui::SmallButton("POWERSHELL")) { std::string error; bool ok = BEditorPlatform_OpenTerminal(state.codeWorkspace.Root(), error); SetMessage(state, ok ? "Opened Project terminal." : error.c_str(), !ok); }
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##CodeFind", "Find", state.codeFind, sizeof(state.codeFind));
        ImGui::SameLine();
        if (ImGui::SmallButton("FIND NEXT") && state.codeFind[0])
        {
            std::size_t start = state.codeSearchOffset;
            std::size_t found = document.text.find(state.codeFind, start);
            if (found == std::string::npos && start) found = document.text.find(state.codeFind);
            if (found == std::string::npos) SetMessage(state, "Search text was not found.", true);
            else { state.codeCursorTarget = static_cast<int>(found); state.codeSearchOffset = found + std::strlen(state.codeFind); }
        }
        ImGui::SameLine(); ImGui::SetNextItemWidth(150.0f); ImGui::InputTextWithHint("##CodeReplace", "Replace with", state.codeReplace, sizeof(state.codeReplace));
        ImGui::SameLine();
        if (ImGui::SmallButton("REPLACE NEXT") && state.codeFind[0])
        {
            std::size_t found = document.text.find(state.codeFind, state.codeSearchOffset);
            if (found == std::string::npos) found = document.text.find(state.codeFind);
            if (found == std::string::npos) SetMessage(state, "Search text was not found.", true);
            else
            {
                std::string replaced = document.text; replaced.replace(found, std::strlen(state.codeFind), state.codeReplace);
                std::string error; if (!state.codeWorkspace.SetText(state.activeCodeTab, replaced, error)) SetMessage(state, error.c_str(), true);
                state.codeSearchOffset = found + std::strlen(state.codeReplace); state.codeCursorTarget = static_cast<int>(state.codeSearchOffset);
            }
        }
        ImGui::SameLine(); ImGui::SetNextItemWidth(90.0f); ImGui::InputInt("##GoLine", &state.codeGoToLine, 0, 0);
        ImGui::SameLine();
        if (ImGui::SmallButton("GO TO LINE"))
        {
            int line = std::max(1, state.codeGoToLine); std::size_t position = 0;
            for (int current = 1; current < line && position < document.text.size(); ++current) { position = document.text.find('\n', position); if (position == std::string::npos) { position = document.text.size(); break; } ++position; }
            state.codeCursorTarget = static_cast<int>(position);
        }
        if (document.externalConflict) ImGui::TextColored(BEditorTheme_GetPalette().error, "EXTERNAL CONFLICT // Save is blocked");
        std::vector<char> buffer(1024 * 1024 + 1, 0);
        std::memcpy(buffer.data(), document.text.data(), std::min(document.text.size(), buffer.size() - 1));
        ImVec2 available = ImGui::GetContentRegionAvail();
        auto navigation = [](ImGuiInputTextCallbackData* data) -> int
        {
            int* target = static_cast<int*>(data->UserData);
            if (*target >= 0) { data->CursorPos = *target; data->SelectionStart = *target; data->SelectionEnd = *target; *target = -1; }
            return 0;
        };
        if (ImGui::InputTextMultiline("##CodeText", buffer.data(), buffer.size(), available,
            ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways, navigation, &state.codeCursorTarget))
        {
            std::string error;
            if (!state.codeWorkspace.SetText(state.activeCodeTab, buffer.data(), error)) SetMessage(state, error.c_str(), true);
        }
    }
    ImGui::EndGroup();
    ImGui::End();
}

static void DrawEditorShell(EditorState& state)
{
    double currentTime = GetTime();

    if (currentTime >= state.nextGitRefreshTime)
    {
        state.gitInitialized = BEditorGit_IsInitialized(state.manifestPath.parent_path());
        state.nextGitRefreshTime = currentTime + 1.0;
    }

    if (currentTime >= state.nextAssetRefreshTime)
    {
        std::uint64_t previousRevision = state.assetService.Revision();
        std::vector<BEditorAssetMove> moves; std::string error;
        if (!state.assetService.Refresh(moves, error) || !ApplyAssetMoves(state, moves, error))
            SetMessage(state, error.c_str(), true);
        if (state.assetService.Revision() != previousRevision)
        {
            BAsciiDrawList_Destroy(&state.viewportDrawList);
            BTextSpriteCache_Destroy(&state.viewportSpriteCache);
            state.viewportRevision = 0;
        }
        state.nextAssetRefreshTime = currentTime + 1.0;
    }
    if (currentTime >= state.nextCodeRefreshTime)
    {
        std::string error;
        if (!state.codeWorkspace.PollExternalChanges(error) && !error.empty()) SetMessage(state, error.c_str(), true);
        state.nextCodeRefreshTime = currentTime + 1.0;
    }

    state.buildService.Update();
    BEditorBuildState buildState = state.buildService.State();

    if (buildState != state.observedBuildState)
    {
        state.observedBuildState = buildState;

        if (buildState == BEditorBuildState::Failed)
        {
            state.uiConfig.showBuildOutput = true;
            state.uiConfig.showProblems = true;
            SetMessage(state, "Build or game process failed. Inspect Build Output and Problems.", true);
        }
        else if (buildState == BEditorBuildState::BuildSucceeded)
            SetMessage(state, "Project build succeeded.", false);
        else if (buildState == BEditorBuildState::Running)
            SetMessage(state, "Project is running.", false);
        else if (buildState == BEditorBuildState::Completed)
            SetMessage(state, "Project process completed.", false);
    }

    DrawEditorMenuBar(state);

    if (state.workspaceSession.IsLoaded() &&
        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        SaveWorkspace(state);
    }
    if (state.workspaceSession.IsLoaded() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        std::string error;
        if (!state.workspaceSession.Undo(error)) SetMessage(state, error.c_str(), true);
    }
    if (state.workspaceSession.IsLoaded() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
    {
        std::string error;
        if (!state.workspaceSession.Redo(error)) SetMessage(state, error.c_str(), true);
    }
    if (state.workspaceSession.IsLoaded() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
    {
        std::string error;
        bool ok = state.workspaceSession.DuplicateSelectedEntity(error);
        SetMessage(state, ok ? "Entity duplicated." : error.c_str(), !ok);
    }
    ImGuiID dockspaceId = ImGui::GetID("BasilEditorDockspace");
    ImGui::DockSpaceOverViewport(
        dockspaceId,
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode
    );

    if (state.resetDockLayout)
    {
        BuildDefaultDockLayout(dockspaceId, state.uiConfig);
        state.resetDockLayout = false;
    }

    DrawProjectDetails(state);
    DrawWorkspaceViewport(state);
    DrawCodeEditor(state);
    BEditorPanelFeedback feedback = BEditorPanels_DrawScaffolds(
        state.uiConfig,
        state.project,
        state.workspaceSession,
        state.assetService,
        state.componentRegistry,
        state.codeWorkspace,
        state.textSpriteDocument,
        state.buildService,
        state.manifestPath.parent_path(),
        state.message,
        state.messageIsError
    );

    if (!feedback.message.empty())
        SetMessage(state, feedback.message.c_str(), feedback.isError);
    if (!feedback.openFile.empty())
    {
        std::string error;
        if (state.codeWorkspace.OpenFile(feedback.openFile, error))
        {
            state.uiConfig.showCodeEditor = true;
            const auto& documents = state.codeWorkspace.Documents();
            for (std::size_t i = 0; i < documents.size(); ++i) if (documents[i].relativePath == feedback.openFile) state.activeCodeTab = i;
        }
        else SetMessage(state, error.c_str(), true);
    }

    if (state.showUIConfigManager)
    {
        if (ImGui::Begin("UI CONFIG MANAGER", &state.showUIConfigManager))
        {
            ImGui::TextUnformatted("Save a reusable global default or an explicit Project-owned layout.");
            ImGui::TextDisabled("GLOBAL // %s", state.globalUIConfigPath.string().c_str());
            ImGui::TextDisabled("PROJECT // .basil/editor-ui.basilui.json");
            ImGui::Separator();
            if (ImGui::Button("SAVE GLOBAL DEFAULT", ImVec2(-1.0f, 0.0f)))
            {
                CaptureUIConfigLayout(state);
                std::string error; bool ok = BEditorUIConfig_Save(state.globalUIConfigPath.string(), state.uiConfig, error);
                SetMessage(state, ok ? "Global UI Config saved." : error.c_str(), !ok);
            }
            if (ImGui::Button("SAVE FOR THIS PROJECT", ImVec2(-1.0f, 0.0f)))
            {
                CaptureUIConfigLayout(state);
                fs::path path = state.manifestPath.parent_path() / ".basil" / "editor-ui.basilui.json";
                std::string error; bool ok = BEditorUIConfig_Save(path.string(), state.uiConfig, error);
                SetMessage(state, ok ? "Project UI Config saved." : error.c_str(), !ok);
            }
            if (ImGui::Button("RESET MAINTAINED DEFAULT", ImVec2(-1.0f, 0.0f)))
            {
                state.uiConfig = BEditorUIConfig_Default(); state.resetDockLayout = true;
                SetMessage(state, "Maintained default UI Config restored.", false);
            }
        }
        ImGui::End();
    }

    if (state.confirmProjectClose)
        ImGui::OpenPopup("UNSAVED PROJECT CHANGES");

    if (ImGui::BeginPopupModal(
        "UNSAVED PROJECT CHANGES",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
    ))
    {
        ImGui::TextUnformatted("The Project has unsaved authoring changes.");
        ImGui::TextDisabled("Save before returning to the Project Browser, discard, or cancel.");
        ImGui::Spacing();

        if (ImGui::Button("SAVE + CLOSE", ImVec2(170.0f, 0.0f)) && SaveWorkspace(state))
        {
            state.confirmProjectClose = false;
            ImGui::CloseCurrentPopup();
            ReturnToProjectBrowser(state);
        }

        ImGui::SameLine();

        if (ImGui::Button("DISCARD", ImVec2(120.0f, 0.0f)))
        {
            std::string ignored;
            state.workspaceSession.DiscardRecovery(ignored);
            state.confirmProjectClose = false;
            state.workspaceSession.Reset();
            state.assetService.Reset();
            state.textSpriteDocument.Reset();
            state.codeWorkspace = BEditorCodeWorkspace{};
            ResetViewportPreview(state);
            state.projectOpen = false;
            SetWindowTitle("BasilEditor");
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("CANCEL", ImVec2(100.0f, 0.0f)))
        {
            state.confirmProjectClose = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (state.confirmRecovery)
        ImGui::OpenPopup("WORKSPACE RECOVERY AVAILABLE");
    if (ImGui::BeginPopupModal("WORKSPACE RECOVERY AVAILABLE", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("A newer recovery snapshot exists for this Workspace.");
        ImGui::TextDisabled("Restore it as unsaved work, or discard the snapshot.");
        if (ImGui::Button("RESTORE", ImVec2(140.0f, 0.0f)))
        {
            std::string error; bool ok = state.workspaceSession.RestoreRecovery(error);
            if (ok) { state.confirmRecovery = false; SetMessage(state, "Workspace recovery restored as unsaved work.", false); ImGui::CloseCurrentPopup(); }
            else SetMessage(state, error.c_str(), true);
        }
        ImGui::SameLine();
        if (ImGui::Button("DISCARD", ImVec2(140.0f, 0.0f)))
        {
            std::string error; bool ok = state.workspaceSession.DiscardRecovery(error);
            if (ok) { state.confirmRecovery = false; SetMessage(state, "Workspace recovery discarded.", false); ImGui::CloseCurrentPopup(); }
            else SetMessage(state, error.c_str(), true);
        }
        ImGui::EndPopup();
    }

    if (state.confirmApplicationClose)
        ImGui::OpenPopup("EXIT WITH UNSAVED CHANGES");
    if (ImGui::BeginPopupModal("EXIT WITH UNSAVED CHANGES", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("The Project has unsaved authoring changes.");
        if (ImGui::Button("SAVE + EXIT", ImVec2(150.0f, 0.0f)) && SaveWorkspace(state))
        {
            state.confirmApplicationClose = false; state.exitApproved = true; ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("DISCARD + EXIT", ImVec2(150.0f, 0.0f)))
        {
            std::string ignored;
            state.workspaceSession.DiscardRecovery(ignored);
            state.confirmApplicationClose = false; state.exitApproved = true; ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("CANCEL", ImVec2(100.0f, 0.0f)))
        {
            state.confirmApplicationClose = false; ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static int RunEditor(int argumentCount, char** arguments)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1100, 700, "BasilEditor");
    ApplyApplicationIcons();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    EditorState state;
    state.recentPath = EditorDataDirectory() / "recent-projects.json";
    state.preferencesPath = EditorDataDirectory() / "preferences.json";
    state.globalUIConfigPath = EditorDataDirectory() / "ui-configs" / "default.basilui.json";
    std::string closeInterceptorError;
    bool closeInterceptorInstalled = BEditorPlatform_InstallCloseInterceptor(GetWindowHandle(), closeInterceptorError);
    std::string preferencesError;
    bool preferencesLoaded = BEditorPreferences_Load(
        state.preferencesPath.string(),
        state.preferences,
        preferencesError
    );

    rlImGuiBeginInitImGui();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    bool bundledFontsLoaded = BEditorTheme_Initialize(state.preferences.interfaceScale);
    rlImGuiEndInitImGui();

    std::string defaultDirectory = DefaultProjectDirectory().string();
    snprintf(state.parentDirectory, sizeof(state.parentDirectory), "%s", defaultDirectory.c_str());

    BProjectError recentError{};
    bool recentProjectsLoaded = BRecentProjects_Load(
        state.recentPath.string().c_str(),
        &state.recent,
        &recentError
    );

    if (!preferencesLoaded)
        SetMessage(state, preferencesError.c_str(), true);
    else if (!recentProjectsLoaded)
        SetMessage(state, recentError.message, true);
    else if (!bundledFontsLoaded)
        SetMessage(state, "JetBrains Mono could not be loaded; BasilEditor is using its fallback font.", true);
    else if (!closeInterceptorInstalled)
        SetMessage(state, closeInterceptorError.c_str(), true);

    if (argumentCount > 1)
        OpenProject(state, arguments[1]);

    while (!state.exitApproved)
    {
        if (state.workspaceSession.IsLoaded() && state.workspaceSession.IsDirty())
        {
            if (state.recoveryObservedRevision != state.workspaceSession.Revision())
            {
                state.recoveryObservedRevision = state.workspaceSession.Revision();
                state.recoveryDueTime = GetTime() + 10.0;
            }
            else if (state.recoveryDueTime > 0.0 && GetTime() >= state.recoveryDueTime)
            {
                std::string recoveryError;
                if (!state.workspaceSession.SaveRecovery(recoveryError))
                    SetMessage(state, recoveryError.c_str(), true);
                state.recoveryDueTime = 0.0;
            }
        }
        BeginDrawing();
        ClearBackground(Color{ 22, 25, 31, 255 });
        rlImGuiBegin();
        if (state.projectOpen)
            DrawEditorShell(state);
        else
            DrawProjectBrowser(state);
        rlImGuiEnd();
        EndDrawing();

        if (BEditorPlatform_TakeCloseRequest() || WindowShouldClose())
        {
            if (state.buildService.IsBusy())
                SetMessage(state, "Stop the active build or game before exiting BasilEditor.", true);
            else if (state.workspaceSession.IsDirty() || state.textSpriteDocument.IsDirty() || state.codeWorkspace.HasDirtyDocuments())
                state.confirmApplicationClose = true;
            else
                state.exitApproved = true;
        }
    }

    rlImGuiShutdown();
    ResetViewportPreview(state);
    BEditorPlatform_RemoveCloseInterceptor();
    CloseWindow();
    return 0;
}

int main(int argumentCount, char** arguments)
{
    try
    {
        return RunEditor(argumentCount, arguments);
    }
    catch (const std::exception& exception)
    {
        std::fprintf(stderr, "BasilEditor fatal error: %s\n", exception.what());
    }
    catch (...)
    {
        std::fprintf(stderr, "BasilEditor fatal error: unknown exception\n");
    }

    if (IsWindowReady())
        CloseWindow();

    return 1;
}
