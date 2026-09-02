#include "BProject.h"
#include "BProjectGenerator.h"
#include "BRecentProjects.h"
#include "BEditorGit.h"
#include "BEditorBuildService.h"
#include "BEditorPanels.h"
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
    bool confirmProjectClose = false;
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
    state.workspaceSession.Reset();
    std::string workspaceError;
    bool workspaceLoaded = state.workspaceSession.Load(
        absolutePath.parent_path(),
        state.project.startupWorkspace,
        workspaceError
    );

    if (!workspaceLoaded)
        state.uiConfig.showProblems = true;

    BRecentProjects_Add(&state.recent, absolutePath.string().c_str());
    SaveRecentProjects(state);
    SetWindowTitle(TextFormat("BasilEditor - %s", state.project.name));
    SetMessage(
        state,
        workspaceLoaded ?
            (state.workspaceSession.RequiresMigration() ?
                "Project opened. Startup Workspace will migrate safely on first save." :
                "Project and startup Workspace opened successfully.") :
            workspaceError.c_str(),
        !workspaceLoaded
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

    if (ImGui::Button("OPEN PROJECT LINK", ImVec2(-1.0f, 0.0f)))
        OpenProject(state, state.openPath);

    ImGui::Spacing();
    ImGui::TextDisabled("Native platform file selection arrives with the platform-integration stage.");
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

    if (state.workspaceSession.IsDirty())
    {
        state.confirmProjectClose = true;
        return;
    }

    state.projectOpen = false;
    state.workspaceSession.Reset();
    ResetViewportPreview(state);
    SetWindowTitle("BasilEditor");
}

static bool SaveWorkspace(EditorState& state)
{
    std::string error;
    bool succeeded = state.workspaceSession.Save(error);
    SetMessage(state, succeeded ? "Workspace saved. A recovery backup was retained." : error.c_str(), !succeeded);
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

    if (state.workspaceSession.IsDirty() && !SaveWorkspace(state))
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

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Workspace"))
    {
        ImGui::BeginDisabled();
        ImGui::MenuItem("New Workspace");
        ImGui::MenuItem("Open Workspace...");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!state.workspaceSession.IsLoaded());

        if (ImGui::MenuItem("Save Workspace", "Ctrl+S"))
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
        ImGui::Separator();

        if (ImGui::MenuItem("Reset Default UI Config"))
        {
            state.uiConfig = BEditorUIConfig_Default();
            state.resetDockLayout = true;
            SetMessage(state, "Default UI Config restored.", false);
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

    const char* statusText = state.workspaceSession.IsDirty() ?
        "WORKSPACE // MODIFIED" : "PROJECT LINK // STABLE";
    float statusWidth = ImGui::CalcTextSize(statusText).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - statusWidth - ImGui::GetStyle().ItemSpacing.x);
    ImGui::TextColored(
        state.workspaceSession.IsDirty() ? palette.warning : palette.success,
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

static void DrawEditorShell(EditorState& state)
{
    double currentTime = GetTime();

    if (currentTime >= state.nextGitRefreshTime)
    {
        state.gitInitialized = BEditorGit_IsInitialized(state.manifestPath.parent_path());
        state.nextGitRefreshTime = currentTime + 1.0;
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
    BEditorPanelFeedback feedback = BEditorPanels_DrawScaffolds(
        state.uiConfig,
        state.project,
        state.workspaceSession,
        state.buildService,
        state.manifestPath.parent_path(),
        state.message,
        state.messageIsError
    );

    if (!feedback.message.empty())
        SetMessage(state, feedback.message.c_str(), feedback.isError);

    if (state.confirmProjectClose)
        ImGui::OpenPopup("UNSAVED WORKSPACE CHANGES");

    if (ImGui::BeginPopupModal(
        "UNSAVED WORKSPACE CHANGES",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
    ))
    {
        ImGui::TextUnformatted("The active Workspace has unsaved changes.");
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
            state.confirmProjectClose = false;
            state.workspaceSession.Reset();
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
}

static int RunEditor(int argumentCount, char** arguments)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1100, 700, "BasilEditor");
    ApplyApplicationIcons();
    SetTargetFPS(60);

    EditorState state;
    state.recentPath = EditorDataDirectory() / "recent-projects.json";
    state.preferencesPath = EditorDataDirectory() / "preferences.json";
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

    if (argumentCount > 1)
        OpenProject(state, arguments[1]);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(Color{ 22, 25, 31, 255 });
        rlImGuiBegin();
        if (state.projectOpen)
            DrawEditorShell(state);
        else
            DrawProjectBrowser(state);
        rlImGuiEnd();
        EndDrawing();
    }

    rlImGuiShutdown();
    ResetViewportPreview(state);
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
