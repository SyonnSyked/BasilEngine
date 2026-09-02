#include "BEditorUIConfig.h"

#include "cJSON.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace
{
bool ReadBool(cJSON* root, const char* name, bool& value)
{
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsBool(item)) return false;
    value = cJSON_IsTrue(item);
    return true;
}
}

BEditorUIConfig BEditorUIConfig_Default()
{
    return BEditorUIConfig{};
}

bool BEditorUIConfig_Validate(const BEditorUIConfig& config, std::string& error)
{
    error.clear();

    if (config.schemaVersion != BEDITOR_UI_CONFIG_SCHEMA_VERSION)
    {
        error = "UI Config uses an unsupported schema version.";
        return false;
    }

    if (config.leftRatio < 0.1f || config.leftRatio > 0.4f ||
        config.rightRatio < 0.1f || config.rightRatio > 0.4f ||
        config.bottomRatio < 0.1f || config.bottomRatio > 0.45f)
    {
        error = "UI Config contains an unsupported panel ratio.";
        return false;
    }

    return true;
}

bool BEditorUIConfig_Load(const std::string& path, BEditorUIConfig& output, std::string& error)
{
    output = BEditorUIConfig_Default();
    error.clear();
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { error = "Could not open UI Config."; return false; }
    std::string contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    cJSON* root = cJSON_Parse(contents.c_str());
    if (!cJSON_IsObject(root)) { cJSON_Delete(root); error = "UI Config contains invalid JSON."; return false; }
    cJSON* schema = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    cJSON* left = cJSON_GetObjectItemCaseSensitive(root, "leftRatio");
    cJSON* right = cJSON_GetObjectItemCaseSensitive(root, "rightRatio");
    cJSON* bottom = cJSON_GetObjectItemCaseSensitive(root, "bottomRatio");
    BEditorUIConfig parsed;
    bool validTypes = cJSON_IsNumber(schema) && schema->valuedouble == static_cast<double>(schema->valueint) &&
        cJSON_IsNumber(left) && cJSON_IsNumber(right) && cJSON_IsNumber(bottom) &&
        ReadBool(root, "showProjectDetails", parsed.showProjectDetails) &&
        ReadBool(root, "showWorkspaceHierarchy", parsed.showWorkspaceHierarchy) &&
        ReadBool(root, "showInspector", parsed.showInspector) &&
        ReadBool(root, "showWorkspaceViewport", parsed.showWorkspaceViewport) &&
        ReadBool(root, "showAssets", parsed.showAssets) && ReadBool(root, "showConsole", parsed.showConsole) &&
        ReadBool(root, "showBuildOutput", parsed.showBuildOutput) && ReadBool(root, "showProblems", parsed.showProblems) &&
        ReadBool(root, "showTerminal", parsed.showTerminal);
    if (!validTypes) { cJSON_Delete(root); error = "UI Config is missing required fields or contains incorrect types."; return false; }
    parsed.schemaVersion = schema->valueint;
    parsed.leftRatio = static_cast<float>(left->valuedouble);
    parsed.rightRatio = static_cast<float>(right->valuedouble);
    parsed.bottomRatio = static_cast<float>(bottom->valuedouble);
    cJSON_Delete(root);
    if (!BEditorUIConfig_Validate(parsed, error)) return false;
    output = parsed;
    return true;
}

bool BEditorUIConfig_Save(const std::string& path, const BEditorUIConfig& config, std::string& error)
{
    if (!BEditorUIConfig_Validate(config, error)) return false;
    fs::path destination(path);
    std::error_code fileError;
    if (!destination.parent_path().empty()) fs::create_directories(destination.parent_path(), fileError);
    if (fileError) { error = "Could not create UI Config directory: " + fileError.message(); return false; }
    cJSON* root = cJSON_CreateObject();
    if (!root) { error = "Out of memory while creating UI Config."; return false; }
    cJSON_AddNumberToObject(root, "schemaVersion", config.schemaVersion);
    cJSON_AddNumberToObject(root, "leftRatio", config.leftRatio);
    cJSON_AddNumberToObject(root, "rightRatio", config.rightRatio);
    cJSON_AddNumberToObject(root, "bottomRatio", config.bottomRatio);
#define BASIL_ADD_BOOL(field) cJSON_AddBoolToObject(root, #field, config.field)
    BASIL_ADD_BOOL(showProjectDetails); BASIL_ADD_BOOL(showWorkspaceHierarchy);
    BASIL_ADD_BOOL(showInspector); BASIL_ADD_BOOL(showWorkspaceViewport);
    BASIL_ADD_BOOL(showAssets); BASIL_ADD_BOOL(showConsole);
    BASIL_ADD_BOOL(showBuildOutput); BASIL_ADD_BOOL(showProblems); BASIL_ADD_BOOL(showTerminal);
#undef BASIL_ADD_BOOL
    char* json = cJSON_Print(root); cJSON_Delete(root);
    if (!json) { error = "Could not serialize UI Config."; return false; }
    fs::path temporary = destination; temporary += ".tmp";
    FILE* file = std::fopen(temporary.string().c_str(), "wb");
    if (!file) { cJSON_free(json); error = "Could not open UI Config for writing."; return false; }
    size_t length = std::char_traits<char>::length(json);
    bool written = std::fwrite(json, 1, length, file) == length && std::fwrite("\n", 1, 1, file) == 1;
    bool complete = std::fclose(file) == 0 && written;
    cJSON_free(json);
    if (!complete) { fs::remove(temporary, fileError); error = "Could not write complete UI Config."; return false; }
    fs::path backup = destination; backup += ".bak";
    if (fs::exists(destination, fileError)) { fs::remove(backup, fileError); fileError.clear(); fs::rename(destination, backup, fileError); }
    if (fileError) { fs::remove(temporary, fileError); error = "Could not back up UI Config."; return false; }
    fs::rename(temporary, destination, fileError);
    if (fileError) { std::error_code ignored; if (fs::exists(backup, ignored)) fs::rename(backup, destination, ignored); error = "Could not replace UI Config: " + fileError.message(); return false; }
    error.clear();
    return true;
}

const char* BEditorPanel_Name(BEditorPanel panel)
{
    switch (panel)
    {
        case BEditorPanel::ProjectDetails: return "PROJECT DETAILS";
        case BEditorPanel::WorkspaceHierarchy: return "WORKSPACE HIERARCHY";
        case BEditorPanel::Inspector: return "INSPECTOR";
        case BEditorPanel::WorkspaceViewport: return "WORKSPACE VIEWPORT";
        case BEditorPanel::Assets: return "ASSETS";
        case BEditorPanel::Console: return "CONSOLE";
        case BEditorPanel::BuildOutput: return "BUILD OUTPUT";
        case BEditorPanel::Problems: return "PROBLEMS";
        case BEditorPanel::Terminal: return "TERMINAL";
    }

    return "UNKNOWN PANEL";
}
