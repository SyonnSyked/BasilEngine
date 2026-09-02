#ifndef BASIL_EDITOR_COMPONENT_REGISTRY_H
#define BASIL_EDITOR_COMPONENT_REGISTRY_H

#include <filesystem>
#include <string>
#include <vector>

enum class BEditorComponentFieldType { Bool, Int, Float, String, Color, EntityReference, AssetReference, Enum };

struct BEditorComponentField
{
    std::string id;
    std::string displayName;
    std::string group;
    BEditorComponentFieldType type = BEditorComponentFieldType::String;
    std::string defaultJson;
    std::vector<std::string> options;
    bool hasMinimum = false;
    bool hasMaximum = false;
    double minimum = 0.0;
    double maximum = 0.0;
};

struct BEditorComponentType
{
    std::string id;
    std::string displayName;
    int version = 1;
    std::vector<BEditorComponentField> fields;
};

class BEditorComponentRegistry
{
public:
    bool Open(const std::filesystem::path& projectRoot, std::string& error);
    const std::vector<BEditorComponentType>& Types() const;
    const BEditorComponentType* Find(const std::string& id) const;
    bool DefaultDataJson(const std::string& id, std::string& json, std::string& error) const;

private:
    std::vector<BEditorComponentType> types_;
};

#endif
