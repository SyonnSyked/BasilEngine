#include "BEditorPreferences.h"

#include "cJSON.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace
{
constexpr std::array<float, 5> SUPPORTED_SCALES{ 1.0f, 1.15f, 1.35f, 1.5f, 1.75f };
constexpr float SCALE_EPSILON = 0.001f;
}

BEditorPreferences BEditorPreferences_Default()
{
    return BEditorPreferences{};
}

bool BEditorPreferences_IsSupportedScale(float scale)
{
    for (float supported : SUPPORTED_SCALES)
    {
        if (std::fabs(scale - supported) < SCALE_EPSILON)
            return true;
    }

    return false;
}

bool BEditorPreferences_Load(
    const std::string& path,
    BEditorPreferences& output,
    std::string& error
)
{
    output = BEditorPreferences_Default();
    error.clear();

    std::error_code fileError;

    if (!fs::exists(path, fileError))
    {
        if (fileError)
        {
            error = "Could not inspect the editor preferences file: " + fileError.message();
            return false;
        }

        return true;
    }

    std::ifstream stream(path, std::ios::binary);

    if (!stream)
    {
        error = "Could not open the editor preferences file.";
        return false;
    }

    std::string contents{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()
    };
    cJSON* root = cJSON_Parse(contents.c_str());

    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        error = "Editor preferences contain invalid JSON.";
        return false;
    }

    cJSON* schemaVersion = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    cJSON* interfaceScale = cJSON_GetObjectItemCaseSensitive(root, "interfaceScale");

    if (!cJSON_IsNumber(schemaVersion) ||
        schemaVersion->valuedouble != static_cast<double>(schemaVersion->valueint) ||
        !cJSON_IsNumber(interfaceScale))
    {
        cJSON_Delete(root);
        error = "Editor preferences are missing required fields or contain incorrect types.";
        return false;
    }

    if (schemaVersion->valueint != 1 && schemaVersion->valueint != BEDITOR_PREFERENCES_SCHEMA_VERSION)
    {
        cJSON_Delete(root);
        error = "Editor preferences use an unsupported schema version.";
        return false;
    }

    float scale = static_cast<float>(interfaceScale->valuedouble);

    if (!BEditorPreferences_IsSupportedScale(scale))
    {
        cJSON_Delete(root);
        error = "Editor preferences contain an unsupported interface scale.";
        return false;
    }

    output.schemaVersion = BEDITOR_PREFERENCES_SCHEMA_VERSION;
    output.interfaceScale = scale;
    if (schemaVersion->valueint >= 2)
    {
        cJSON* externalEditor = cJSON_GetObjectItemCaseSensitive(root, "externalEditor");
        cJSON* terminal = cJSON_GetObjectItemCaseSensitive(root, "terminal");
        if (!cJSON_IsString(externalEditor) || !cJSON_IsString(terminal) || !externalEditor->valuestring[0] ||
            !terminal->valuestring[0] || std::char_traits<char>::length(externalEditor->valuestring) > 1024 ||
            std::char_traits<char>::length(terminal->valuestring) > 1024)
        { cJSON_Delete(root); output = BEditorPreferences_Default(); error = "Editor tool commands are missing or invalid."; return false; }
        output.externalEditor = externalEditor->valuestring;
        output.terminal = terminal->valuestring;
    }
    cJSON_Delete(root);
    return true;
}

bool BEditorPreferences_Save(
    const std::string& path,
    const BEditorPreferences& preferences,
    std::string& error
)
{
    error.clear();

    if (preferences.schemaVersion != BEDITOR_PREFERENCES_SCHEMA_VERSION ||
        !BEditorPreferences_IsSupportedScale(preferences.interfaceScale) || preferences.externalEditor.empty() ||
        preferences.terminal.empty() || preferences.externalEditor.size() > 1024 || preferences.terminal.size() > 1024)
    {
        error = "Editor preferences contain unsupported values.";
        return false;
    }

    fs::path destination(path);
    std::error_code directoryError;
    fs::create_directories(destination.parent_path(), directoryError);

    if (directoryError)
    {
        error = "Could not create the editor settings directory: " + directoryError.message();
        return false;
    }

    cJSON* root = cJSON_CreateObject();

    if (root == nullptr)
    {
        error = "Out of memory while creating editor preferences.";
        return false;
    }

    cJSON_AddNumberToObject(root, "schemaVersion", preferences.schemaVersion);
    cJSON_AddNumberToObject(root, "interfaceScale", preferences.interfaceScale);
    cJSON_AddStringToObject(root, "externalEditor", preferences.externalEditor.c_str());
    cJSON_AddStringToObject(root, "terminal", preferences.terminal.c_str());
    char* json = cJSON_Print(root);
    cJSON_Delete(root);

    if (json == nullptr)
    {
        error = "Could not serialize editor preferences.";
        return false;
    }

    fs::path temporary = destination;
    temporary += ".tmp";
    FILE* file = std::fopen(temporary.string().c_str(), "wb");

    if (file == nullptr)
    {
        cJSON_free(json);
        error = "Could not open editor preferences for writing.";
        return false;
    }

    size_t length = std::char_traits<char>::length(json);
    bool written = std::fwrite(json, 1, length, file) == length &&
        std::fwrite("\n", 1, 1, file) == 1;
    bool closed = std::fclose(file) == 0;
    cJSON_free(json);

    if (!written || !closed)
    {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        error = "Could not write complete editor preferences.";
        return false;
    }

    std::error_code replaceError;
    fs::remove(destination, replaceError);
    replaceError.clear();
    fs::rename(temporary, destination, replaceError);

    if (replaceError)
    {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        error = "Could not replace editor preferences: " + replaceError.message();
        return false;
    }

    return true;
}
