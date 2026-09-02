#ifndef BASIL_EDITOR_PREFERENCES_H
#define BASIL_EDITOR_PREFERENCES_H

#include <string>

constexpr int BEDITOR_PREFERENCES_SCHEMA_VERSION = 1;
constexpr float BEDITOR_DEFAULT_INTERFACE_SCALE = 1.35f;

struct BEditorPreferences
{
    int schemaVersion = BEDITOR_PREFERENCES_SCHEMA_VERSION;
    float interfaceScale = BEDITOR_DEFAULT_INTERFACE_SCALE;
};

BEditorPreferences BEditorPreferences_Default();
bool BEditorPreferences_IsSupportedScale(float scale);

// A missing file is a successful load of defaults. Malformed or unsupported
// data returns false, leaves defaults in output, and provides an explanation.
bool BEditorPreferences_Load(
    const std::string& path,
    BEditorPreferences& output,
    std::string& error
);

bool BEditorPreferences_Save(
    const std::string& path,
    const BEditorPreferences& preferences,
    std::string& error
);

#endif
