#include "BProject.h"
#include "BProjectGenerator.h"
#include "BRecentProjects.h"
#include "BEditorTheme.h"

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static constexpr float EDITOR_UI_SCALE = 1.35f;

struct EditorState
{
    BRecentProjects recent{};
    BProject project{};
    fs::path manifestPath;
    fs::path recentPath;
    char openPath[BPROJECT_PATH_MAX]{};
    char projectName[BPROJECT_NAME_MAX] = "My Basil Game";
    char identifier[BPROJECT_IDENTIFIER_MAX] = "MyBasilGame";
    char parentDirectory[BPROJECT_PATH_MAX]{};
    int languageMode = static_cast<int>(BPROJECT_LANGUAGE_MIXED);
    int cStandardIndex = 2;
    int cppStandardIndex = 6;
    bool initializeGit = false;
    bool projectOpen = false;
    std::string message;
    bool messageIsError = false;
};

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
    BProjectError error{};
    std::error_code pathError;
    fs::path absolutePath = fs::absolute(manifestPath, pathError).lexically_normal();

    if (pathError || !BProject_Load(absolutePath.string().c_str(), &state.project, &error))
    {
        SetMessage(state, pathError ? pathError.message().c_str() : error.message, true);
        return false;
    }

    state.manifestPath = absolutePath;
    state.projectOpen = true;
    BRecentProjects_Add(&state.recent, absolutePath.string().c_str());
    SaveRecentProjects(state);
    SetWindowTitle(TextFormat("BasilEditor - %s", state.project.name));
    SetMessage(state, "Project opened successfully.", false);
    return true;
}

static bool InitializeGit(const fs::path& projectRoot)
{
    std::error_code error;
    fs::path previous = fs::current_path(error);

    if (error)
        return false;

    fs::current_path(projectRoot, error);

    if (error)
        return false;

    int result = std::system("git init");
    fs::current_path(previous, error);
    return result == 0 && !error;
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

    bool gitFailed = state.initializeGit && !InitializeGit(root);

    if (OpenProject(state, root / (std::string(project.identifier) + ".basilproject")) && gitFailed)
        SetMessage(state, "The project was created, but Git initialization failed. You can retry it from the project overview.", true);
}

static void DrawMessage(const EditorState& state)
{
    if (state.message.empty())
        return;

    ImVec4 color = state.messageIsError ? ImVec4(1.0f, 0.35f, 0.3f, 1.0f) : ImVec4(0.35f, 0.85f, 0.5f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", state.message.c_str());
    ImGui::PopStyleColor();
}

static void DrawProjectBrowser(EditorState& state)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("BasilEngine Project Browser", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::TextUnformatted("BasilEngine");
    ImGui::TextDisabled("Create a project or open an existing .basilproject manifest.");
    ImGui::Separator();

    if (ImGui::BeginTabBar("ProjectActions"))
    {
        if (ImGui::BeginTabItem("Recent Projects"))
        {
            if (state.recent.count == 0)
                ImGui::TextDisabled("No recent projects yet.");

            for (size_t i = 0; i < state.recent.count; ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                fs::path path(state.recent.paths[i]);
                if (ImGui::Button(path.stem().string().c_str(), ImVec2(220, 0)))
                    OpenProject(state, path);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", state.recent.paths[i]);
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove"))
                {
                    BRecentProjects_Remove(&state.recent, i);
                    SaveRecentProjects(state);
                    --i;
                }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Open Project"))
        {
            ImGui::InputText("Manifest path", state.openPath, sizeof(state.openPath));
            ImGui::SameLine();
            if (ImGui::Button("Open"))
                OpenProject(state, state.openPath);
            ImGui::TextDisabled("Enter the path to a .basilproject file. A native file picker will follow in the platform-integration stage.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("New Project"))
        {
            ImGui::InputText("Project name", state.projectName, sizeof(state.projectName));
            ImGui::InputText("Code identifier", state.identifier, sizeof(state.identifier));
            ImGui::InputText("Location", state.parentDirectory, sizeof(state.parentDirectory));
            const char* modes[] = { "C only", "C++ only", "C and C++" };
            const char* cStandards[] = { "C90", "C99", "C11", "C17", "C23" };
            const char* cppStandards[] = { "C++98", "C++11", "C++14", "C++17", "C++20", "C++23", "C++26" };
            ImGui::Combo("Languages", &state.languageMode, modes, IM_ARRAYSIZE(modes));
            if (state.languageMode != static_cast<int>(BPROJECT_LANGUAGE_CPP))
                ImGui::Combo("C standard", &state.cStandardIndex, cStandards, IM_ARRAYSIZE(cStandards));
            if (state.languageMode != static_cast<int>(BPROJECT_LANGUAGE_C))
                ImGui::Combo("C++ standard", &state.cppStandardIndex, cppStandards, IM_ARRAYSIZE(cppStandards));
            ImGui::Checkbox("Initialize a Git repository", &state.initializeGit);
            if (ImGui::Button("Create and Open Project"))
                CreateProject(state);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    DrawMessage(state);
    ImGui::End();
}

static void DrawProjectOverview(EditorState& state)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Project Overview", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::Text("%s", state.project.name);
    ImGui::Separator();
    ImGui::Text("Manifest: %s", state.manifestPath.string().c_str());
    ImGui::Text("Identifier: %s", state.project.identifier);
    ImGui::Text("Languages: %s", BProject_LanguageModeToString(state.project.languageMode));
    ImGui::Text("C standard: %d", state.project.cStandard);
    ImGui::Text("C++ standard: %d", state.project.cppStandard);
    ImGui::Spacing();
    if (ImGui::Button("Initialize Git Here"))
    {
        bool succeeded = InitializeGit(state.manifestPath.parent_path());
        SetMessage(state, succeeded ? "Git repository initialized." : "Git initialization failed. Verify that Git is installed and available on PATH.", !succeeded);
    }
    ImGui::SameLine();
    if (ImGui::Button("Back to Project Browser"))
    {
        state.projectOpen = false;
        SetWindowTitle("BasilEditor");
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Scene editing, build, and run controls arrive in the next editor stages.");
    DrawMessage(state);
    ImGui::End();
}

int main(int argumentCount, char** arguments)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1100, 700, "BasilEditor");
    SetTargetFPS(60);
    rlImGuiBeginInitImGui();
    bool bundledFontsLoaded = BEditorTheme_Initialize(EDITOR_UI_SCALE);
    rlImGuiEndInitImGui();

    EditorState state;
    state.recentPath = EditorDataDirectory() / "recent-projects.json";
    std::string defaultDirectory = DefaultProjectDirectory().string();
    snprintf(state.parentDirectory, sizeof(state.parentDirectory), "%s", defaultDirectory.c_str());

    BProjectError recentError{};
    if (!BRecentProjects_Load(state.recentPath.string().c_str(), &state.recent, &recentError))
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
            DrawProjectOverview(state);
        else
            DrawProjectBrowser(state);
        rlImGuiEnd();
        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
