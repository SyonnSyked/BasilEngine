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

BEditorPanelFeedback DrawWorkspaceHierarchy(
    BEditorUIConfig& config,
    const BProject& project,
    BEditorWorkspaceSession& session
)
{
    if (!config.showWorkspaceHierarchy)
        return {};

    BEditorPanelFeedback feedback;

    if (ImGui::Begin(
        BEditorPanel_Name(BEditorPanel::WorkspaceHierarchy),
        &config.showWorkspaceHierarchy
    ))
    {
        const BEditorThemePalette& palette = BEditorTheme_GetPalette();
        ImGui::TextColored(palette.violet, "ACTIVE WORKSPACE");
        ImGui::Separator();
        ImGui::TextDisabled("SOURCE // %s", project.startupWorkspace);
        ImGui::TextColored(
            session.IsDirty() ? palette.warning : palette.success,
            session.IsDirty() ? "MODIFIED" : "SAVED"
        );
        ImGui::Separator();

        if (!session.IsLoaded())
        {
            ImGui::TextColored(palette.error, "[!] STARTUP WORKSPACE UNAVAILABLE");
            ImGui::End();
            return feedback;
        }

        if (ImGui::Button("+ ADD ENTITY", ImVec2(-1.0f, 0.0f)))
        {
            std::string error;
            bool succeeded = session.AddEntity(error);
            feedback.message = succeeded ? "Workspace entity created." : error;
            feedback.isError = !succeeded;
        }

        ImGui::Spacing();
        const BWorkspace& workspace = session.Workspace();

        if (ImGui::TreeNodeEx(workspace.name, ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (workspace.entityCount == 0)
                ImGui::TextColored(palette.cyan, "[ EMPTY ENTITY GRAPH ]");

            for (size_t i = 0; i < workspace.entityCount; ++i)
            {
                const BWorkspaceEntity& entity = workspace.entities[i];
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth;

                if (session.SelectedIndex() == i)
                    flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx(reinterpret_cast<void*>(i + 1), flags, "%s", entity.name);

                if (ImGui::IsItemClicked())
                    session.Select(i);
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
    return feedback;
}

BEditorPanelFeedback DrawInspector(BEditorUIConfig& config, BEditorWorkspaceSession& session)
{
    if (!config.showInspector)
        return {};

    BEditorPanelFeedback feedback;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Inspector), &config.showInspector))
    {
        BWorkspaceEntity* entity = session.MutableSelectedEntity();

        if (entity == nullptr)
            DrawPanelState("SELECTION // NONE", "Select a Workspace entity in the Hierarchy.");
        else
        {
            const BEditorThemePalette& palette = BEditorTheme_GetPalette();
            ImGui::TextColored(palette.violet, "ENTITY INSPECTOR");
            ImGui::Separator();
            ImGui::TextDisabled("STABLE ID");
            ImGui::TextWrapped("%s", entity->id);
            ImGui::Spacing();

            if (ImGui::InputText("Name", entity->name, sizeof(entity->name)))
            {
                session.MarkDirty();
                feedback.message = "Entity name modified.";
            }

            if (ImGui::Checkbox("Enabled", &entity->enabled))
            {
                session.MarkDirty();
                feedback.message = "Entity enabled state modified.";
            }

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, palette.error);

            if (ImGui::Button("DELETE ENTITY", ImVec2(-1.0f, 0.0f)))
            {
                std::string error;
                bool succeeded = session.RemoveSelectedEntity(error);
                feedback.message = succeeded ? "Workspace entity deleted." : error;
                feedback.isError = !succeeded;
            }

            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
    return feedback;
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

void DrawBuildOutput(BEditorUIConfig& config, const BEditorBuildService& service)
{
    if (!config.showBuildOutput)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::BuildOutput), &config.showBuildOutput))
    {
        const BEditorThemePalette& palette = BEditorTheme_GetPalette();
        ImVec4 stateColor = service.State() == BEditorBuildState::Failed ? palette.error :
            service.IsBusy() ? palette.warning : palette.success;
        ImGui::TextColored(stateColor, "BUILD SERVICE // %s", service.StateLabel());
        ImGui::Separator();
        ImGui::BeginChild("##BuildOutputStream", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (service.Output().empty())
            ImGui::TextDisabled("Build output will appear here.");
        else
            ImGui::TextUnformatted(service.Output().c_str());

        ImGui::EndChild();
    }

    ImGui::End();
}

void DrawProblems(BEditorUIConfig& config, const BEditorBuildService& service)
{
    if (!config.showProblems)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Problems), &config.showProblems))
    {
        const BEditorThemePalette& palette = BEditorTheme_GetPalette();
        ImGui::TextColored(
            service.Problems().empty() ? palette.success : palette.error,
            "DIAGNOSTICS // %zu",
            service.Problems().size()
        );
        ImGui::Separator();

        if (service.Problems().empty())
            ImGui::TextDisabled("No build errors detected.");
        else
        {
            for (const std::string& problem : service.Problems())
                ImGui::BulletText("%s", problem.c_str());
        }
    }

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

BEditorPanelFeedback BEditorPanels_DrawScaffolds(
    BEditorUIConfig& config,
    const BProject& project,
    BEditorWorkspaceSession& workspaceSession,
    const BEditorBuildService& buildService,
    const fs::path& projectRoot,
    const std::string& editorMessage,
    bool messageIsError
)
{
    BEditorPanelFeedback feedback = DrawWorkspaceHierarchy(config, project, workspaceSession);
    BEditorPanelFeedback inspectorFeedback = DrawInspector(config, workspaceSession);

    if (!inspectorFeedback.message.empty())
        feedback = inspectorFeedback;

    DrawAssets(config, projectRoot);
    DrawConsole(config, editorMessage, messageIsError);
    DrawBuildOutput(config, buildService);
    DrawProblems(config, buildService);
    DrawTerminal(config, projectRoot);
    return feedback;
}
