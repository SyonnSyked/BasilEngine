#include "BEditorPanels.h"

#include "BEditorTheme.h"

#include "imgui.h"

#include <algorithm>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace
{
void DrawPanelState(const char* state, const char* explanation)
{
    const BEditorThemePalette& palette = BEditorTheme_GetPalette();
    ImGui::TextColored(palette.violet, "%s", state);
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("%s", explanation);
}

void DrawWorkspaceHierarchy(BEditorUIConfig& config, const BProject& project)
{
    if (!config.showWorkspaceHierarchy)
        return;

    if (ImGui::Begin(
        BEditorPanel_Name(BEditorPanel::WorkspaceHierarchy),
        &config.showWorkspaceHierarchy
    ))
    {
        const BEditorThemePalette& palette = BEditorTheme_GetPalette();
        ImGui::TextColored(palette.violet, "ACTIVE WORKSPACE");
        ImGui::Separator();
        ImGui::TextDisabled("SOURCE");
        ImGui::TextWrapped("%s", project.startupWorkspace);
        ImGui::Spacing();
        std::string workspaceLabel = fs::path(project.startupWorkspace).stem().string();

        if (ImGui::TreeNodeEx(workspaceLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextColored(palette.cyan, "[ EMPTY ENTITY GRAPH ]");
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void DrawInspector(BEditorUIConfig& config)
{
    if (!config.showInspector)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Inspector), &config.showInspector))
    {
        DrawPanelState(
            "SELECTION // NONE",
            "Select a Workspace entity or asset when those data models become editable."
        );
    }

    ImGui::End();
}

void DrawAssets(BEditorUIConfig& config, const fs::path& projectRoot)
{
    if (!config.showAssets)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Assets), &config.showAssets))
    {
        const BEditorThemePalette& palette = BEditorTheme_GetPalette();
        fs::path assetRoot = projectRoot / "assets";
        ImGui::TextColored(palette.violet, "PROJECT ASSET ROOT");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", assetRoot.string().c_str());
        ImGui::Separator();

        std::error_code error;
        std::vector<fs::directory_entry> entries;
        bool assetDirectoryExists = fs::is_directory(assetRoot, error);

        if (assetDirectoryExists)
        {
            for (fs::directory_iterator iterator(assetRoot, error), end;
                !error && iterator != end;
                iterator.increment(error))
            {
                entries.push_back(*iterator);
            }
        }

        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right)
        {
            return left.path().filename().string() < right.path().filename().string();
        });

        if (error)
            ImGui::TextColored(palette.error, "[!] Could not inspect the asset directory: %s", error.message().c_str());
        else if (!assetDirectoryExists)
            ImGui::TextColored(palette.warning, "[!] ASSET DIRECTORY MISSING");
        else if (entries.empty())
            ImGui::TextColored(palette.cyan, "[ EMPTY ASSET DIRECTORY ]");
        else
        {
            for (const fs::directory_entry& entry : entries)
            {
                std::error_code typeError;
                bool directory = entry.is_directory(typeError);
                ImGui::BulletText("%s %s", directory ? "[DIR]" : "[FILE]", entry.path().filename().string().c_str());
            }
        }
    }

    ImGui::End();
}

void DrawConsole(BEditorUIConfig& config, const std::string& message, bool isError)
{
    if (!config.showConsole)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Console), &config.showConsole))
    {
        const BEditorThemePalette& palette = BEditorTheme_GetPalette();
        ImGui::TextColored(palette.violet, "EDITOR EVENT STREAM");
        ImGui::Separator();

        if (message.empty())
            ImGui::TextDisabled("No editor events recorded for this session.");
        else
            ImGui::TextColored(isError ? palette.error : palette.success, "%s", message.c_str());
    }

    ImGui::End();
}

void DrawBuildOutput(BEditorUIConfig& config)
{
    if (!config.showBuildOutput)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::BuildOutput), &config.showBuildOutput))
        DrawPanelState("BUILD SERVICE // OFFLINE", "Build output will stream here after build/run integration.");

    ImGui::End();
}

void DrawProblems(BEditorUIConfig& config)
{
    if (!config.showProblems)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Problems), &config.showProblems))
        DrawPanelState("DIAGNOSTICS // CLEAR", "No diagnostic provider is connected yet.");

    ImGui::End();
}

void DrawTerminal(BEditorUIConfig& config, const fs::path& projectRoot)
{
    if (!config.showTerminal)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Terminal), &config.showTerminal))
    {
#ifdef _WIN32
        const char* shell = "Windows PowerShell";
#else
        const char* shell = "User login shell";
#endif
        DrawPanelState("TERMINAL HOST // RESERVED", "Process hosting is not connected in this scaffold step.");
        ImGui::Spacing();
        ImGui::TextDisabled("PLANNED DEFAULT");
        ImGui::TextUnformatted(shell);
        ImGui::TextDisabled("WORKING DIRECTORY");
        ImGui::TextWrapped("%s", projectRoot.string().c_str());
    }

    ImGui::End();
}
}

void BEditorPanels_DrawScaffolds(
    BEditorUIConfig& config,
    const BProject& project,
    const fs::path& projectRoot,
    const std::string& editorMessage,
    bool messageIsError
)
{
    DrawWorkspaceHierarchy(config, project);
    DrawInspector(config);
    DrawAssets(config, projectRoot);
    DrawConsole(config, editorMessage, messageIsError);
    DrawBuildOutput(config);
    DrawProblems(config);
    DrawTerminal(config, projectRoot);
}
