#include "BEditorPreferences.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

int main()
{
    int failures = 0;
    BEditorPreferences defaults = BEditorPreferences_Default();
    failures += Check(defaults.interfaceScale == 1.35f, "default scale is 135 percent");
    failures += Check(BEditorPreferences_IsSupportedScale(1.0f), "100 percent is supported");
    failures += Check(BEditorPreferences_IsSupportedScale(1.75f), "175 percent is supported");
    failures += Check(!BEditorPreferences_IsSupportedScale(1.2f), "arbitrary scales are rejected");

    auto unique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    fs::path root = fs::temp_directory_path() / ("basil-editor-preferences-" + std::to_string(unique));
    fs::path path = root / "preferences.json";
    std::string error;
    BEditorPreferences loaded;

    failures += Check(BEditorPreferences_Load(path.string(), loaded, error), "missing file loads defaults");
    failures += Check(loaded.interfaceScale == 1.35f, "missing file returns default scale");

    BEditorPreferences saved = BEditorPreferences_Default();
    saved.externalEditor = "nvim.exe";
    saved.terminal = "pwsh.exe";
    const float scales[] = { 1.0f, 1.15f, 1.35f, 1.5f, 1.75f };

    for (float scale : scales)
    {
        saved.interfaceScale = scale;
        failures += Check(BEditorPreferences_Save(path.string(), saved, error), "preferences save");
        failures += Check(BEditorPreferences_Load(path.string(), loaded, error), "saved preferences load");
        failures += Check(loaded.interfaceScale == scale, "scale preset round trips");
        failures += Check(loaded.externalEditor == "nvim.exe" && loaded.terminal == "pwsh.exe", "tool commands round trip");
    }

    {
        std::ofstream invalid(path, std::ios::binary | std::ios::trunc);
        invalid << "{ not json";
    }

    failures += Check(!BEditorPreferences_Load(path.string(), loaded, error), "invalid JSON is rejected");
    failures += Check(loaded.interfaceScale == 1.35f, "invalid JSON leaves safe defaults");
    failures += Check(!error.empty(), "invalid JSON provides an explanation");

    std::error_code ignored;
    fs::remove_all(root, ignored);

    if (failures == 0)
        std::cout << "BEditorPreferencesTests passed.\n";

    return failures == 0 ? 0 : 1;
}
