#include "BEditorPanels.h"

#include "BEditorPlatformDialogs.h"
#include "BEditorTheme.h"
#include "BEditorPlatformDialogs.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {
bool ContainsInsensitive(const std::string &value, const char *filter)
{
    if (filter == nullptr || filter[0] == '\0')
        return true;
    std::string needle(filter);
    std::string haystack(value);
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return haystack.find(needle) != std::string::npos;
}
void DrawPanelState(const char *state, const char *explanation)
{
    const BEditorThemePalette &palette = BEditorTheme_GetPalette();
    ImGui::TextColored(palette.violet, "%s", state);
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("%s", explanation);
}

void SetFeedback(BEditorPanelFeedback &feedback, bool succeeded, const char *success,
                 const std::string &error)
{
    feedback.message = succeeded ? success : error;
    feedback.isError = !succeeded;
}

BEditorPanelFeedback DrawWorkspaceHierarchy(BEditorUIConfig &config, const BProject &project,
                                            BEditorWorkspaceSession &session,
                                            BEditorAssetService &assetService)
{
    if (!config.showWorkspaceHierarchy)
        return {};

    BEditorPanelFeedback feedback;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::WorkspaceHierarchy),
                     &config.showWorkspaceHierarchy)) {
        const BEditorThemePalette &palette = BEditorTheme_GetPalette();
        ImGui::TextColored(palette.violet, "ACTIVE WORKSPACE");
        ImGui::Separator();
        std::string activeWorkspace = session.RelativePath();
        bool isStartup = !activeWorkspace.empty() && activeWorkspace == project.startupWorkspace;

        ImGui::TextDisabled("SOURCE // %s",
                            activeWorkspace.empty() ? "(none)" : activeWorkspace.c_str());

        ImGui::TextDisabled("STARTUP // %s", isStartup ? "YES" : "NO");
        ImGui::TextColored(session.IsDirty() ? palette.warning : palette.success,
                           session.IsDirty() ? "MODIFIED" : "SAVED");
        ImGui::Separator();

        if (!session.IsLoaded()) {
            ImGui::TextColored(palette.error, "[!] CURRENT WORKSPACE UNAVAILABLE");
            ImGui::End();
            return feedback;
        }

        if (ImGui::Button("+ ADD ENTITY", ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("AddEntityPopup");

        static char entityFilter[128]{};
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##EntityFilter", "Filter entities...", entityFilter,
                                 sizeof(entityFilter));

        if (ImGui::BeginPopup("AddEntityPopup")) {
            ImGui::TextColored(palette.violet, "VISIBLE GLYPH");
            for (int glyph = 0x20; glyph <= 0x7e; ++glyph) {
                char label[16];
                std::snprintf(label, sizeof(label), "%c##glyph%d", glyph == ' ' ? '_' : glyph,
                              glyph);
                if (ImGui::SmallButton(label)) {
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

            bool foundTextSprite = false;

            for (const BEditorAssetRecord &record : assetService.Records()) {
                if (record.kind != BEditorAssetKind::TextSprite) {
                    continue;
                }

                foundTextSprite = true;

                if (ImGui::Selectable(record.relativePath.c_str())) {
                    std::string error;

                    bool succeeded =
                        session.AddTextSpriteEntity(record.id, record.relativePath, error);

                    SetFeedback(feedback, succeeded, "Text Sprite entity created.", error);

                    if (succeeded)
                        ImGui::CloseCurrentPopup();
                }
            }

            if (!foundTextSprite) {
                ImGui::TextDisabled("No .txt assets found under assets/.");
            }

            ImGui::Separator();
            if (ImGui::Selectable("EMPTY ENTITY // TRANSFORM ONLY")) {
                std::string error;
                bool succeeded = session.AddEmptyEntity(error);
                SetFeedback(feedback, succeeded, "Empty entity created.", error);
                if (succeeded)
                    ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        const BWorkspaceDocument &workspace = session.Workspace();

        if (ImGui::TreeNodeEx(workspace.name, ImGuiTreeNodeFlags_DefaultOpen)) {
            if (workspace.entityCount == 0)
                ImGui::TextColored(palette.cyan, "[ EMPTY ENTITY GRAPH ]");

            for (size_t i = 0; i < workspace.entityCount; ++i) {
                const BWorkspaceEntity &entity = workspace.entities[i];
                if (!ContainsInsensitive(entity.name, entityFilter) &&
                    !ContainsInsensitive(entity.id, entityFilter))
                    continue;
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                           ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                           ImGuiTreeNodeFlags_SpanAvailWidth;

                if (session.SelectedIndex() == i)
                    flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx(reinterpret_cast<void *>(i + 1), flags, "%s", entity.name);

                if (ImGui::IsItemClicked())
                    session.Select(i);
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
    return feedback;
}

BEditorPanelFeedback DrawInspector(BEditorUIConfig &config, BEditorWorkspaceSession &session,
                                   const BEditorComponentRegistry &registry,
                                   BEditorAssetService &assetService)
{
    if (!config.showInspector)
        return {};

    BEditorPanelFeedback feedback;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Inspector), &config.showInspector)) {
        BWorkspaceEntity *entity = session.MutableSelectedEntity();

        if (entity == nullptr)
            DrawPanelState("SELECTION // NONE", "Select a Workspace entity in the Hierarchy.");
        else {
            const BEditorThemePalette &palette = BEditorTheme_GetPalette();
            ImGui::TextColored(palette.violet, "ENTITY INSPECTOR");
            ImGui::Separator();
            ImGui::TextDisabled("STABLE ID");
            ImGui::TextWrapped("%s", entity->id);
            ImGui::Spacing();

            static char editedName[BWORKSPACE_ENTITY_NAME_MAX]{};
            static char editedEntityId[BWORKSPACE_ENTITY_ID_MAX]{};
            if (std::strcmp(editedEntityId, entity->id) != 0) {
                std::snprintf(editedEntityId, sizeof(editedEntityId), "%s", entity->id);
                std::snprintf(editedName, sizeof(editedName), "%s", entity->name);
            }
            if (ImGui::InputText("Name", editedName, sizeof(editedName),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string error;
                bool succeeded = session.SetSelectedName(editedName, error);
                if (!succeeded)
                    std::snprintf(editedName, sizeof(editedName), "%s", entity->name);
                SetFeedback(feedback, succeeded, "Entity name modified.", error);
            }

            bool enabled = entity->enabled;
            if (ImGui::Checkbox("Enabled", &enabled)) {
                std::string error;
                SetFeedback(feedback, session.SetSelectedEnabled(enabled, error),
                            "Entity enabled state modified.", error);
            }

            BWorkspaceComponent *transform =
                BWorkspaceEntity_FindComponent(entity, BWORKSPACE_TRANSFORM2D_TYPE);
            BWorkspaceComponent *renderComponent =
                BWorkspaceEntity_FindComponent(entity, BWORKSPACE_ASCII_RENDERABLE_TYPE);
            if (transform != nullptr) {
                ImGui::SeparatorText("TRANSFORM2D");
                float position[2] = {transform->data.transform2d.x, transform->data.transform2d.y};
                if (ImGui::DragFloat2("Position", position, 0.1f)) {
                    std::string error;
                    SetFeedback(feedback,
                                session.SetSelectedTransform({position[0], position[1]}, error),
                                "Position modified.", error);
                }
            }

            if (renderComponent != nullptr) {
                ImGui::SeparatorText("ASCII RENDERABLE");
                BAsciiRenderable renderable = renderComponent->data.asciiRenderable;
                const char *kinds[] = {"Glyph", "Text Sprite"};
                int kind = static_cast<int>(renderable.sourceKind);
                bool changed = ImGui::Combo("Source", &kind, kinds, 2);
                if (changed) {
                    renderable.sourceKind = static_cast<BAsciiSourceKind>(kind);
                    if (renderable.sourceKind == BASCII_SOURCE_GLYPH &&
                        (renderable.glyph < 0x20 || renderable.glyph > 0x7e))
                        renderable.glyph = '@';
                    if (renderable.sourceKind == BASCII_SOURCE_TEXT_SPRITE &&
                        renderable.textSprite.id[0] == '\0') {
                        for (const BEditorAssetRecord &record : assetService.Records()) {
                            if (record.kind != BEditorAssetKind::TextSprite)
                                continue;

                            BDiagnosticList diagnostics{};

                            if (BAssetRef_Set(&renderable.textSprite, record.id.c_str(),
                                              record.relativePath.c_str(), &diagnostics))
                                break;
                        }
                    }
                }

                if (renderable.sourceKind == BASCII_SOURCE_GLYPH) {
                    int glyph = static_cast<unsigned char>(renderable.glyph) - 0x20;
                    std::string preview(1, renderable.glyph == ' ' ? '_' : renderable.glyph);
                    if (ImGui::BeginCombo("Glyph", preview.c_str())) {
                        for (int value = 0x20; value <= 0x7e; ++value) {
                            char label[8];
                            std::snprintf(label, sizeof(label), "%c##inspect%d",
                                          value == ' ' ? '_' : value, value);
                            if (ImGui::Selectable(label, glyph == value - 0x20)) {
                                renderable.glyph = static_cast<char>(value);
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    const char *preview = renderable.textSprite.path[0] != '\0'
                                              ? renderable.textSprite.path
                                              : "SELECT .TXT ASSET";

                    if (ImGui::BeginCombo("Text Sprite", preview)) {
                        for (const BEditorAssetRecord &record : assetService.Records()) {
                            if (record.kind != BEditorAssetKind::TextSprite) {
                                continue;
                            }

                            bool selected = record.id == renderable.textSprite.id;

                            if (ImGui::Selectable(record.relativePath.c_str(), selected)) {
                                BDiagnosticList diagnostics{};

                                if (BAssetRef_Set(&renderable.textSprite, record.id.c_str(),
                                                  record.relativePath.c_str(), &diagnostics)) {
                                    changed = true;
                                } else {
                                    const BDiagnostic *diagnostic =
                                        BDiagnosticList_FirstError(&diagnostics);

                                    feedback = {diagnostic != nullptr
                                                    ? diagnostic->message
                                                    : "Could not assign Text Sprite asset.",
                                                true};
                                }
                            }

                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                float foreground[4] = {
                    renderable.foreground.r / 255.0f, renderable.foreground.g / 255.0f,
                    renderable.foreground.b / 255.0f, renderable.foreground.a / 255.0f};
                float background[4] = {
                    renderable.background.r / 255.0f, renderable.background.g / 255.0f,
                    renderable.background.b / 255.0f, renderable.background.a / 255.0f};
                if (ImGui::ColorEdit4("Foreground", foreground)) {
                    renderable.foreground = {
                        static_cast<unsigned char>(std::lround(foreground[0] * 255.0f)),
                        static_cast<unsigned char>(std::lround(foreground[1] * 255.0f)),
                        static_cast<unsigned char>(std::lround(foreground[2] * 255.0f)),
                        static_cast<unsigned char>(std::lround(foreground[3] * 255.0f))};
                    changed = true;
                }
                if (ImGui::ColorEdit4("Background", background)) {
                    renderable.background = {
                        static_cast<unsigned char>(std::lround(background[0] * 255.0f)),
                        static_cast<unsigned char>(std::lround(background[1] * 255.0f)),
                        static_cast<unsigned char>(std::lround(background[2] * 255.0f)),
                        static_cast<unsigned char>(std::lround(background[3] * 255.0f))};
                    changed = true;
                }
                int layer = renderable.layer;
                if (ImGui::InputInt("Layer", &layer)) {
                    if (layer >= -32768 && layer <= 32767) {
                        renderable.layer = static_cast<short>(layer);
                        changed = true;
                    } else {
                        feedback = {"Layer must be between -32768 and 32767.", true};
                    }
                }
                const char *anchors[] = {"Bottom Center", "Center", "Top Left"};
                int anchor = static_cast<int>(renderable.anchor);
                if (ImGui::Combo("Anchor", &anchor, anchors, 3)) {
                    renderable.anchor = static_cast<BAsciiAnchor>(anchor);
                    changed = true;
                }
                if (ImGui::Checkbox("Visible", &renderable.visible))
                    changed = true;
                if (ImGui::Checkbox("Transparent Spaces", &renderable.transparentSpaces))
                    changed = true;
                if (changed) {
                    std::string error;
                    SetFeedback(feedback, session.SetSelectedRenderable(renderable, error),
                                "ASCII Renderable modified.", error);
                }
            }

            for (const BEditorComponentType &type : registry.Types()) {
                BWorkspaceComponent *component =
                    BWorkspaceEntity_FindComponent(entity, type.id.c_str());
                if (component) {
                    ImGui::SeparatorText(type.displayName.c_str());
                    for (const BEditorComponentField &field : type.fields)
                        ImGui::TextDisabled("%s // %s", field.displayName.c_str(),
                                            field.id.c_str());
                    ImGui::TextWrapped("%s", component->data.unknownDataJson);
                }
            }

            if (!registry.Types().empty() &&
                ImGui::BeginCombo("ADD PROJECT COMPONENT", "SELECT TYPE")) {
                for (const BEditorComponentType &type : registry.Types()) {
                    bool exists =
                        BWorkspaceEntity_FindComponent(entity, type.id.c_str()) != nullptr;
                    if (ImGui::Selectable(type.displayName.c_str(), false,
                                          exists ? ImGuiSelectableFlags_Disabled : 0)) {
                        std::string data;
                        std::string error;
                        bool succeeded =
                            registry.DefaultDataJson(type.id, data, error) &&
                            session.AddSelectedCustomComponent(type.id, type.version, data, error);
                        SetFeedback(feedback, succeeded, "Project component added.", error);
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, palette.error);

            if (ImGui::Button("DELETE ENTITY", ImVec2(-1.0f, 0.0f))) {
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

BEditorPanelFeedback DrawAssets(BEditorUIConfig &config, const fs::path &projectRoot,
                                BEditorAssetService &assetService,
                                BEditorTextSpriteDocument &textDocument,
                                BEditorWorkspaceSession &workspaceSession)
{
    if (!config.showAssets)
        return {};

    BEditorPanelFeedback feedback;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Assets), &config.showAssets)) {
        const BEditorThemePalette &palette = BEditorTheme_GetPalette();
        fs::path assetRoot = projectRoot / "assets";
        ImGui::TextColored(palette.violet, "PROJECT ASSET ROOT");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", assetRoot.string().c_str());
        ImGui::Separator();

        static char assetFilter[128]{};
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##AssetFilter", "Filter assets...", assetFilter,
                                 sizeof(assetFilter));

        if (ImGui::Button("NEW TEXT SPRITE", ImVec2(-1.0f, 0.0f))) {
            fs::path selected = assetRoot / "sprite.txt";
            std::string error;
            if (BEditorDialog_SaveTextSprite(selected, error)) {
                fs::path relative =
                    selected.lexically_normal().lexically_relative(projectRoot.lexically_normal());
                bool ok = textDocument.Create(projectRoot, relative.generic_string(), error);
                feedback = {ok ? "Text Sprite created." : error, !ok};
                if (ok)
                    config.showTextSpriteEditor = true;
            } else if (!error.empty())
                feedback = {error, true};
        }
        ImGui::Separator();
        if (assetService.Records().empty())
            ImGui::TextColored(palette.cyan, "[ EMPTY ASSET DIRECTORY ]");
        else {
            bool assetListChanged = false;
            for (const BEditorAssetRecord &record : assetService.Records()) {
                ImGui::PushID(record.id.c_str());
                if (!ContainsInsensitive(record.relativePath, assetFilter))
                    continue;
                std::string label = "[" + std::string(BEditorAssetService::KindName(record.kind)) +
                                    "] " + record.relativePath;
                bool selected =
                    textDocument.IsOpen() && textDocument.RelativePath() == record.relativePath;
                if (ImGui::Selectable(label.c_str(), selected) &&
                    record.kind == BEditorAssetKind::TextSprite) {
                    std::string error;
                    bool ok = textDocument.Open(projectRoot, record.relativePath, error);
                    feedback = {ok ? "Text Sprite opened." : error, !ok};
                    if (ok)
                        config.showTextSpriteEditor = true;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("ID // %s", record.id.c_str());
                if (ImGui::BeginPopupContextItem("AssetActions")) {
                    static char renamed[BPROJECT_PATH_MAX]{};
                    static std::string renameId;
                    if (renameId != record.id) {
                        renameId = record.id;
                        std::snprintf(renamed, sizeof(renamed), "%s",
                                      fs::path(record.relativePath).filename().string().c_str());
                    }
                    ImGui::InputText("New filename", renamed, sizeof(renamed));
                    bool blocked = textDocument.IsDirty() &&
                                   textDocument.RelativePath() == record.relativePath;
                    ImGui::BeginDisabled(blocked);
                    if (ImGui::Button("RENAME")) {
                        std::vector<BEditorAssetMove> moves;
                        std::string error;
                        bool ok = assetService.Rename(record.relativePath, renamed, moves, error);
                        if (ok)
                            for (const auto &move : moves)
                                ok = workspaceSession.RemapAssetPath(move.oldPath, move.newPath,
                                                                     error) &&
                                     ok;
                        if (ok && textDocument.IsOpen() &&
                            textDocument.RelativePath() == record.relativePath)
                            ok = textDocument.Open(
                                projectRoot,
                                moves.empty() ? record.relativePath : moves.front().newPath, error);
                        feedback = {ok ? "Asset renamed and references updated." : error, !ok};
                        if (ok) {
                            assetListChanged = true;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndDisabled();
                    if (blocked)
                        ImGui::TextDisabled("Save the open Text Sprite before renaming it.");
                    ImGui::EndPopup();
                }
                ImGui::PopID();
                if (assetListChanged)
                    break;
            }
        }
    }

    ImGui::End();
    return feedback;
}

BEditorPanelFeedback DrawTextSpriteEditor(BEditorUIConfig &config,
                                          BEditorTextSpriteDocument &document)
{
    if (!config.showTextSpriteEditor)
        return {};
    BEditorPanelFeedback feedback;
    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::TextSpriteEditor),
                     &config.showTextSpriteEditor)) {
        const BEditorThemePalette &palette = BEditorTheme_GetPalette();
        if (!document.IsOpen())
            DrawPanelState("TEXT SPRITE // NONE", "Select or create a .txt asset in Assets.");
        else {
            ImGui::TextColored(document.IsDirty() ? palette.warning : palette.success, "%s // %s",
                               document.IsDirty() ? "MODIFIED" : "SAVED",
                               document.RelativePath().c_str());
            if (document.HasExternalChange()) {
                ImGui::TextColored(palette.warning, "[!] FILE CHANGED OUTSIDE BASILEDITOR");
                if (ImGui::Button("RELOAD DISK COPY")) {
                    std::string error;
                    bool ok = document.Reload(error);
                    feedback = {ok ? "External Text Sprite reloaded." : error, !ok};
                }
                ImGui::SameLine();
                if (ImGui::Button("KEEP EDITOR COPY"))
                    document.AcceptExternalChange();
            }
            if (ImGui::Button("SAVE TEXT SPRITE", ImVec2(-1.0f, 0.0f))) {
                std::string error;
                bool ok = document.Save(error);
                feedback = {ok ? "Text Sprite saved." : error, !ok};
            }
            ImGui::Separator();
            ImVec2 editorSize(ImGui::GetContentRegionAvail().x * 0.55f,
                              ImGui::GetContentRegionAvail().y);
            if (ImGui::InputTextMultiline("##TextSpriteSource", document.Buffer(),
                                          document.Capacity(), editorSize)) {
                document.MarkEdited();
                std::string error;
                if (!document.RefreshPreview(error))
                    feedback = {error, true};
            }
            ImGui::SameLine();
            ImGui::BeginGroup();
            const BTextSprite &sprite = document.Preview();
            ImGui::TextColored(palette.violet, "PREVIEW // %zu x %zu", sprite.width, sprite.height);
            ImGui::TextDisabled("Spaces render as transparent dots.");
            ImGui::BeginChild("##TextSpritePreview", ImVec2(0.0f, 0.0f), true,
                              ImGuiWindowFlags_HorizontalScrollbar);
            for (std::size_t row = 0; row < sprite.height; ++row) {
                std::string line;
                for (std::size_t column = 0; column < sprite.width; ++column) {
                    char glyph = BTextSprite_Cell(&sprite, column, row);
                    line.push_back(glyph == ' ' ? '.' : glyph);
                }
                ImGui::TextColored(palette.cyan, "%s", line.c_str());
            }
            ImGui::EndChild();
            ImGui::EndGroup();
        }
    }
    ImGui::End();
    return feedback;
}

void DrawConsole(BEditorUIConfig &config, const std::string &message, bool isError)
{
    if (!config.showConsole)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Console), &config.showConsole)) {
        const BEditorThemePalette &palette = BEditorTheme_GetPalette();
        ImGui::TextColored(palette.violet, "EDITOR EVENT STREAM");
        ImGui::Separator();

        if (message.empty())
            ImGui::TextDisabled("No editor events recorded for this session.");
        else
            ImGui::TextColored(isError ? palette.error : palette.success, "%s", message.c_str());
    }

    ImGui::End();
}

void DrawBuildOutput(BEditorUIConfig &config, const BEditorBuildService &service)
{
    if (!config.showBuildOutput)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::BuildOutput), &config.showBuildOutput)) {
        const BEditorThemePalette &palette = BEditorTheme_GetPalette();
        ImVec4 stateColor = service.State() == BEditorBuildState::Failed ? palette.error
                            : service.IsBusy()                           ? palette.warning
                                                                         : palette.success;
        ImGui::TextColored(stateColor, "BUILD SERVICE // %s", service.StateLabel());
        ImGui::Separator();
        ImGui::BeginChild("##BuildOutputStream", ImVec2(0.0f, 0.0f), false,
                          ImGuiWindowFlags_HorizontalScrollbar);

        if (service.Output().empty())
            ImGui::TextDisabled("Build output will appear here.");
        else
            ImGui::TextUnformatted(service.Output().c_str());

        ImGui::EndChild();
    }

    ImGui::End();
}

BEditorPanelFeedback DrawProblems(BEditorUIConfig &config, const BEditorBuildService &service,
                                  const BEditorCodeWorkspace &codeWorkspace)
{
    if (!config.showProblems)
        return {};
    BEditorPanelFeedback feedback;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Problems), &config.showProblems)) {
        const BEditorThemePalette &palette = BEditorTheme_GetPalette();
        ImGui::TextColored(service.Problems().empty() ? palette.success : palette.error,
                           "DIAGNOSTICS // %zu", service.Problems().size());
        ImGui::Separator();

        if (service.Problems().empty())
            ImGui::TextDisabled("No build errors detected.");
        else {
            for (const std::string &problem : service.Problems()) {
                if (ImGui::Selectable(problem.c_str()))
                    for (const std::string &path : codeWorkspace.Files())
                        if (std::size_t position = problem.find(path);
                            position != std::string::npos) {
                            feedback.openFile = path;
                            position += path.size();
                            if (position < problem.size() && problem[position] == ':')
                                feedback.openLine = std::atoi(problem.c_str() + position + 1);
                            break;
                        }
            }
        }
    }

    ImGui::End();
    return feedback;
}

void DrawTerminal(BEditorUIConfig &config, const fs::path &projectRoot,
                  BEditorTerminalService &service, const std::string &shell)
{
    if (!config.showTerminal)
        return;

    if (ImGui::Begin(BEditorPanel_Name(BEditorPanel::Terminal), &config.showTerminal)) {
        if (!service.IsRunning()) {
            std::string error;
            if (!service.Start(shell, projectRoot, error))
                DrawPanelState("TERMINAL OFFLINE", error.c_str());
        }
        static char command[2048]{};
        ImGui::TextDisabled("SHELL // %s", shell.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("ROOT // %s", projectRoot.string().c_str());
        if (ImGui::Button("CLEAR"))
            service.Clear();
        ImGui::SameLine();
        if (ImGui::Button("RESTART")) {
            std::string error;
            service.Restart(error);
        }
        ImGui::Separator();
        float inputHeight = ImGui::GetFrameHeightWithSpacing();
        const std::string &output = service.Output();
        std::vector<char> terminalBuffer(output.begin(), output.end());
        terminalBuffer.push_back('\0');
        ImGui::InputTextMultiline("##TerminalOutput", terminalBuffer.data(), terminalBuffer.size(),
                                  ImVec2(0.0f, -inputHeight), ImGuiInputTextFlags_ReadOnly);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##TerminalInput", "Enter command and press Return", command,
                                     sizeof(command), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string error;
            if (service.Send(command, error))
                command[0] = '\0';
        }
    }

    ImGui::End();
}
} // namespace

BEditorPanelFeedback BEditorPanels_DrawScaffolds(
    BEditorUIConfig &config, const BProject &project, BEditorWorkspaceSession &workspaceSession,
    BEditorAssetService &assetService, const BEditorComponentRegistry &componentRegistry,
    BEditorCodeWorkspace &codeWorkspace, BEditorTerminalService &terminalService,
    const std::string &terminalCommand, BEditorTextSpriteDocument &textSpriteDocument,
    const BEditorBuildService &buildService, const fs::path &projectRoot,
    const std::string &editorMessage, bool messageIsError)
{
    BEditorPanelFeedback feedback =
        DrawWorkspaceHierarchy(config, project, workspaceSession, assetService);
    BEditorPanelFeedback inspectorFeedback =
        DrawInspector(config, workspaceSession, componentRegistry, assetService);

    if (!inspectorFeedback.message.empty())
        feedback = inspectorFeedback;

    BEditorPanelFeedback assetFeedback =
        DrawAssets(config, projectRoot, assetService, textSpriteDocument, workspaceSession);
    BEditorPanelFeedback textFeedback = DrawTextSpriteEditor(config, textSpriteDocument);
    if (!assetFeedback.message.empty())
        feedback = assetFeedback;
    if (!textFeedback.message.empty())
        feedback = textFeedback;
    DrawConsole(config, editorMessage, messageIsError);
    DrawBuildOutput(config, buildService);
    BEditorPanelFeedback problemFeedback = DrawProblems(config, buildService, codeWorkspace);
    if (!problemFeedback.openFile.empty())
        feedback.openFile = problemFeedback.openFile;
    if (problemFeedback.openLine > 0)
        feedback.openLine = problemFeedback.openLine;
    DrawTerminal(config, projectRoot, terminalService, terminalCommand);
    return feedback;
}
