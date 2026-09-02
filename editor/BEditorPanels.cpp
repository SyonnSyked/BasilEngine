#include "BEditorPanels.h"

#include "BEditorTheme.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace
{
bool ContainsInsensitive(const std::string& value, const char* filter)
{
    if (filter == nullptr || filter[0] == '\0') return true;
    std::string needle(filter);
    std::string haystack(value);
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return haystack.find(needle) != std::string::npos;
}
void DrawPanelState(const char* state, const char* explanation)
{
    const BEditorThemePalette& palette = BEditorTheme_GetPalette();
    ImGui::TextColored(palette.violet, "%s", state);
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("%s", explanation);
}

std::vector<std::string> FindTextSprites(const fs::path& projectRoot)
{
    std::vector<std::string> paths;
    std::error_code error;
    fs::path assetRoot = projectRoot / "assets";
    if (!fs::is_directory(assetRoot, error))
        return paths;
    for (fs::recursive_directory_iterator iterator(assetRoot, error), end;
        !error && iterator != end; iterator.increment(error))
    {
        std::error_code typeError;
        if (iterator->is_regular_file(typeError) && iterator->path().extension() == ".txt")
            paths.push_back(iterator->path().lexically_relative(projectRoot).generic_string());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

void SetFeedback(BEditorPanelFeedback& feedback, bool succeeded, const char* success, const std::string& error)
{
    feedback.message = succeeded ? success : error;
    feedback.isError = !succeeded;
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
            ImGui::OpenPopup("AddEntityPopup");

        static char entityFilter[128]{};
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##EntityFilter", "Filter entities...", entityFilter, sizeof(entityFilter));

        if (ImGui::BeginPopup("AddEntityPopup"))
        {
            ImGui::TextColored(palette.violet, "VISIBLE GLYPH");
            for (int glyph = 0x20; glyph <= 0x7e; ++glyph)
            {
                char label[16];
                std::snprintf(label, sizeof(label), "%c##glyph%d", glyph == ' ' ? '_' : glyph, glyph);
                if (ImGui::SmallButton(label))
                {
                    std::string error;
                    bool succeeded = session.AddGlyphEntity(static_cast<char>(glyph), error);
                    SetFeedback(feedback, succeeded, "Visible glyph entity created.", error);
                    if (succeeded)
                        ImGui::CloseCurrentPopup();
                }
                if ((glyph - 0x20) % 12 != 11 && glyph != 0x7e)
                    ImGui::SameLine();
            }
            ImGui::SeparatorText("TEXT SPRITES");
            std::vector<std::string> sprites = FindTextSprites(session.ProjectRoot());
            if (sprites.empty())
                ImGui::TextDisabled("No .txt assets found under assets/.");
            for (const std::string& path : sprites)
            {
                if (ImGui::Selectable(path.c_str()))
                {
                    std::string error;
                    bool succeeded = session.AddTextSpriteEntity(path, error);
                    SetFeedback(feedback, succeeded, "Text Sprite entity created.", error);
                    if (succeeded)
                        ImGui::CloseCurrentPopup();
                }
            }
            ImGui::Separator();
            if (ImGui::Selectable("EMPTY ENTITY // TRANSFORM ONLY"))
            {
                std::string error;
                bool succeeded = session.AddEmptyEntity(error);
                SetFeedback(feedback, succeeded, "Empty entity created.", error);
                if (succeeded)
                    ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        const BWorkspaceDocument& workspace = session.Workspace();

        if (ImGui::TreeNodeEx(workspace.name, ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (workspace.entityCount == 0)
                ImGui::TextColored(palette.cyan, "[ EMPTY ENTITY GRAPH ]");

            for (size_t i = 0; i < workspace.entityCount; ++i)
            {
                const BWorkspaceEntity& entity = workspace.entities[i];
                if (!ContainsInsensitive(entity.name, entityFilter) && !ContainsInsensitive(entity.id, entityFilter))
                    continue;
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

            static char editedName[BWORKSPACE_ENTITY_NAME_MAX]{};
            static char editedEntityId[BWORKSPACE_ENTITY_ID_MAX]{};
            if (std::strcmp(editedEntityId, entity->id) != 0)
            {
                std::snprintf(editedEntityId, sizeof(editedEntityId), "%s", entity->id);
                std::snprintf(editedName, sizeof(editedName), "%s", entity->name);
            }
            if (ImGui::InputText("Name", editedName, sizeof(editedName), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string error;
                bool succeeded = session.SetSelectedName(editedName, error);
                if (!succeeded)
                    std::snprintf(editedName, sizeof(editedName), "%s", entity->name);
                SetFeedback(feedback, succeeded, "Entity name modified.", error);
            }

            bool enabled = entity->enabled;
            if (ImGui::Checkbox("Enabled", &enabled))
            {
                std::string error;
                SetFeedback(feedback, session.SetSelectedEnabled(enabled, error), "Entity enabled state modified.", error);
            }

            BWorkspaceComponent* transform = BWorkspaceEntity_FindComponent(entity, BWORKSPACE_TRANSFORM2D_TYPE);
            BWorkspaceComponent* renderComponent = BWorkspaceEntity_FindComponent(entity, BWORKSPACE_ASCII_RENDERABLE_TYPE);
            if (transform != nullptr)
            {
                ImGui::SeparatorText("TRANSFORM2D");
                float position[2] = { transform->data.transform2d.x, transform->data.transform2d.y };
                if (ImGui::DragFloat2("Position", position, 0.1f))
                {
                    std::string error;
                    SetFeedback(feedback, session.SetSelectedTransform({ position[0], position[1] }, error), "Position modified.", error);
                }
            }

            if (renderComponent != nullptr)
            {
                ImGui::SeparatorText("ASCII RENDERABLE");
                BAsciiRenderable renderable = renderComponent->data.asciiRenderable;
                const char* kinds[] = { "Glyph", "Text Sprite" };
                int kind = static_cast<int>(renderable.sourceKind);
                bool changed = ImGui::Combo("Source", &kind, kinds, 2);
                if (changed)
                {
                    renderable.sourceKind = static_cast<BAsciiSourceKind>(kind);
                    if (renderable.sourceKind == BASCII_SOURCE_GLYPH && (renderable.glyph < 0x20 || renderable.glyph > 0x7e))
                        renderable.glyph = '@';
                    std::vector<std::string> sprites = FindTextSprites(session.ProjectRoot());
                    if (renderable.sourceKind == BASCII_SOURCE_TEXT_SPRITE && renderable.textSpritePath[0] == '\0' && !sprites.empty())
                        std::snprintf(renderable.textSpritePath, sizeof(renderable.textSpritePath), "%s", sprites.front().c_str());
                }

                if (renderable.sourceKind == BASCII_SOURCE_GLYPH)
                {
                    int glyph = static_cast<unsigned char>(renderable.glyph) - 0x20;
                    std::string preview(1, renderable.glyph == ' ' ? '_' : renderable.glyph);
                    if (ImGui::BeginCombo("Glyph", preview.c_str()))
                    {
                        for (int value = 0x20; value <= 0x7e; ++value)
                        {
                            char label[8];
                            std::snprintf(label, sizeof(label), "%c##inspect%d", value == ' ' ? '_' : value, value);
                            if (ImGui::Selectable(label, glyph == value - 0x20))
                            {
                                renderable.glyph = static_cast<char>(value);
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    std::vector<std::string> sprites = FindTextSprites(session.ProjectRoot());
                    if (ImGui::BeginCombo("Text Sprite", renderable.textSpritePath[0] ? renderable.textSpritePath : "SELECT .TXT ASSET"))
                    {
                        for (const std::string& path : sprites)
                        {
                            if (ImGui::Selectable(path.c_str(), path == renderable.textSpritePath))
                            {
                                std::snprintf(renderable.textSpritePath, sizeof(renderable.textSpritePath), "%s", path.c_str());
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                float foreground[4] = { renderable.foreground.r / 255.0f, renderable.foreground.g / 255.0f, renderable.foreground.b / 255.0f, renderable.foreground.a / 255.0f };
                float background[4] = { renderable.background.r / 255.0f, renderable.background.g / 255.0f, renderable.background.b / 255.0f, renderable.background.a / 255.0f };
                if (ImGui::ColorEdit4("Foreground", foreground))
                {
                    renderable.foreground = { static_cast<unsigned char>(std::lround(foreground[0] * 255.0f)), static_cast<unsigned char>(std::lround(foreground[1] * 255.0f)), static_cast<unsigned char>(std::lround(foreground[2] * 255.0f)), static_cast<unsigned char>(std::lround(foreground[3] * 255.0f)) };
                    changed = true;
                }
                if (ImGui::ColorEdit4("Background", background))
                {
                    renderable.background = { static_cast<unsigned char>(std::lround(background[0] * 255.0f)), static_cast<unsigned char>(std::lround(background[1] * 255.0f)), static_cast<unsigned char>(std::lround(background[2] * 255.0f)), static_cast<unsigned char>(std::lround(background[3] * 255.0f)) };
                    changed = true;
                }
                int layer = renderable.layer;
                if (ImGui::InputInt("Layer", &layer))
                {
                    if (layer >= -32768 && layer <= 32767)
                    {
                        renderable.layer = static_cast<short>(layer);
                        changed = true;
                    }
                    else
                    {
                        feedback = { "Layer must be between -32768 and 32767.", true };
                    }
                }
                const char* anchors[] = { "Bottom Center", "Center", "Top Left" };
                int anchor = static_cast<int>(renderable.anchor);
                if (ImGui::Combo("Anchor", &anchor, anchors, 3)) { renderable.anchor = static_cast<BAsciiAnchor>(anchor); changed = true; }
                if (ImGui::Checkbox("Visible", &renderable.visible)) changed = true;
                if (ImGui::Checkbox("Transparent Spaces", &renderable.transparentSpaces)) changed = true;
                if (changed)
                {
                    std::string error;
                    SetFeedback(feedback, session.SetSelectedRenderable(renderable, error), "ASCII Renderable modified.", error);
                }
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

        static char assetFilter[128]{};
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##AssetFilter", "Filter assets...", assetFilter, sizeof(assetFilter));

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
                if (!ContainsInsensitive(entry.path().filename().string(), assetFilter))
                    continue;
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
