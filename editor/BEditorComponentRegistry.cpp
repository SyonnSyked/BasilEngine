#include "BEditorComponentRegistry.h"

#include "cJSON.h"

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

namespace
{
bool Identifier(const char* value)
{
    if (!value || !std::isalpha(static_cast<unsigned char>(*value))) return false;
    for (const char* p = value; *p; ++p)
        if (!std::isalnum(static_cast<unsigned char>(*p)) && *p != '.' && *p != '-' && *p != '_') return false;
    return true;
}

bool ExactFields(const cJSON* object, const std::set<std::string>& allowed)
{
    for (const cJSON* child = object ? object->child : nullptr; child; child = child->next)
        if (!child->string || !allowed.count(child->string)) return false;
    return true;
}

bool ParseType(const char* value, BEditorComponentFieldType& result)
{
    const std::pair<const char*, BEditorComponentFieldType> values[] = {
        {"bool", BEditorComponentFieldType::Bool}, {"int", BEditorComponentFieldType::Int},
        {"float", BEditorComponentFieldType::Float}, {"string", BEditorComponentFieldType::String},
        {"color", BEditorComponentFieldType::Color}, {"entity-ref", BEditorComponentFieldType::EntityReference},
        {"asset-ref", BEditorComponentFieldType::AssetReference}, {"enum", BEditorComponentFieldType::Enum}
    };
    if (!value) return false;
    for (const auto& entry : values) if (std::string(value) == entry.first) { result = entry.second; return true; }
    return false;
}

bool ValidDefault(BEditorComponentFieldType type, const cJSON* value, const std::vector<std::string>& options)
{
    if (!value) return false;
    if (type == BEditorComponentFieldType::Bool) return cJSON_IsBool(value);
    if (type == BEditorComponentFieldType::Int) return cJSON_IsNumber(value) && value->valuedouble == value->valueint;
    if (type == BEditorComponentFieldType::Float) return cJSON_IsNumber(value);
    if (type == BEditorComponentFieldType::String || type == BEditorComponentFieldType::Color ||
        type == BEditorComponentFieldType::EntityReference || type == BEditorComponentFieldType::AssetReference)
        return cJSON_IsString(value);
    if (type == BEditorComponentFieldType::Enum && cJSON_IsString(value))
        for (const auto& option : options) if (option == value->valuestring) return true;
    return false;
}
}

bool BEditorComponentRegistry::Open(const std::filesystem::path& projectRoot, std::string& error)
{
    const std::filesystem::path registryPath = projectRoot / ".basil/components.json";
    if (!std::filesystem::exists(registryPath))
    {
        types_.clear();
        error.clear();
        return true;
    }
    std::ifstream stream(registryPath, std::ios::binary);
    if (!stream) { error = "Could not open .basil/components.json."; return false; }
    std::ostringstream contents; contents << stream.rdbuf();
    cJSON* root = cJSON_Parse(contents.str().c_str());
    cJSON* schema = root ? cJSON_GetObjectItemCaseSensitive(root, "schemaVersion") : nullptr;
    cJSON* types = root ? cJSON_GetObjectItemCaseSensitive(root, "types") : nullptr;
    if (!root || !cJSON_IsObject(root) || !ExactFields(root, {"schemaVersion", "types"}) ||
        !cJSON_IsNumber(schema) || schema->valueint != 1 || !cJSON_IsArray(types) || cJSON_GetArraySize(types) > 128)
    { cJSON_Delete(root); error = "Component registry must use schemaVersion 1 and a bounded types array."; return false; }

    std::vector<BEditorComponentType> parsed; std::set<std::string> typeIds;
    for (cJSON* item = types->child; item; item = item->next)
    {
        cJSON* id = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "displayName");
        cJSON* version = cJSON_GetObjectItemCaseSensitive(item, "version");
        cJSON* fields = cJSON_GetObjectItemCaseSensitive(item, "fields");
        if (!cJSON_IsObject(item) || !ExactFields(item, {"id", "displayName", "version", "fields"}) ||
            !cJSON_IsString(id) || !Identifier(id->valuestring) || !typeIds.insert(id->valuestring).second ||
            !cJSON_IsString(name) || !cJSON_IsNumber(version) || version->valueint < 1 ||
            !cJSON_IsArray(fields) || cJSON_GetArraySize(fields) > 64)
        { cJSON_Delete(root); error = "Component type metadata is invalid or duplicated."; return false; }
        BEditorComponentType component{id->valuestring, name->valuestring, version->valueint, {}};
        std::set<std::string> fieldIds;
        for (cJSON* fieldItem = fields->child; fieldItem; fieldItem = fieldItem->next)
        {
            cJSON* fieldId = cJSON_GetObjectItemCaseSensitive(fieldItem, "id");
            cJSON* fieldName = cJSON_GetObjectItemCaseSensitive(fieldItem, "displayName");
            cJSON* fieldType = cJSON_GetObjectItemCaseSensitive(fieldItem, "type");
            cJSON* defaultValue = cJSON_GetObjectItemCaseSensitive(fieldItem, "default");
            cJSON* options = cJSON_GetObjectItemCaseSensitive(fieldItem, "options");
            cJSON* group = cJSON_GetObjectItemCaseSensitive(fieldItem, "group");
            cJSON* minimum = cJSON_GetObjectItemCaseSensitive(fieldItem, "min");
            cJSON* maximum = cJSON_GetObjectItemCaseSensitive(fieldItem, "max");
            BEditorComponentField field;
            if (!cJSON_IsObject(fieldItem) || !ExactFields(fieldItem, {"id", "displayName", "type", "default", "options", "group", "min", "max"}) ||
                !cJSON_IsString(fieldId) || !Identifier(fieldId->valuestring) || !fieldIds.insert(fieldId->valuestring).second ||
                !cJSON_IsString(fieldName) || (group && !cJSON_IsString(group)) ||
                !cJSON_IsString(fieldType) || !ParseType(fieldType->valuestring, field.type))
            { cJSON_Delete(root); error = "Component field metadata is invalid or duplicated."; return false; }
            field.group = group ? group->valuestring : "General";
            bool numeric = field.type == BEditorComponentFieldType::Int || field.type == BEditorComponentFieldType::Float;
            if ((minimum || maximum) && !numeric) { cJSON_Delete(root); error = "Only numeric fields may declare min or max."; return false; }
            if ((minimum && !cJSON_IsNumber(minimum)) || (maximum && !cJSON_IsNumber(maximum)) ||
                (minimum && maximum && minimum->valuedouble > maximum->valuedouble))
            { cJSON_Delete(root); error = "Component field bounds are invalid."; return false; }
            if (minimum) { field.hasMinimum = true; field.minimum = minimum->valuedouble; }
            if (maximum) { field.hasMaximum = true; field.maximum = maximum->valuedouble; }
            if (field.type == BEditorComponentFieldType::Enum)
            {
                if (!cJSON_IsArray(options) || cJSON_GetArraySize(options) < 1 || cJSON_GetArraySize(options) > 64)
                { cJSON_Delete(root); error = "Enum fields require a bounded options array."; return false; }
                for (cJSON* option = options->child; option; option = option->next)
                    if (cJSON_IsString(option)) field.options.emplace_back(option->valuestring); else { cJSON_Delete(root); error = "Enum options must be strings."; return false; }
            }
            else if (options) { cJSON_Delete(root); error = "Only enum fields may declare options."; return false; }
            if (!ValidDefault(field.type, defaultValue, field.options) ||
                (minimum && defaultValue->valuedouble < minimum->valuedouble) ||
                (maximum && defaultValue->valuedouble > maximum->valuedouble))
            { cJSON_Delete(root); error = "Component field default does not match its type or bounds."; return false; }
            char* printed = cJSON_PrintUnformatted(defaultValue);
            if (!printed) { cJSON_Delete(root); error = "Out of memory reading component defaults."; return false; }
            field.id = fieldId->valuestring; field.displayName = fieldName->valuestring; field.defaultJson = printed; cJSON_free(printed);
            component.fields.push_back(std::move(field));
        }
        parsed.push_back(std::move(component));
    }
    cJSON_Delete(root); types_ = std::move(parsed); error.clear(); return true;
}

const std::vector<BEditorComponentType>& BEditorComponentRegistry::Types() const { return types_; }
const BEditorComponentType* BEditorComponentRegistry::Find(const std::string& id) const
{ for (const auto& type : types_) if (type.id == id) return &type; return nullptr; }

bool BEditorComponentRegistry::DefaultDataJson(const std::string& id, std::string& json, std::string& error) const
{
    const BEditorComponentType* type = Find(id);
    if (!type) { error = "Component type is not registered."; return false; }
    cJSON* object = cJSON_CreateObject();
    for (const auto& field : type->fields)
    {
        cJSON* value = cJSON_Parse(field.defaultJson.c_str());
        if (!value || !cJSON_AddItemToObject(object, field.id.c_str(), value)) { cJSON_Delete(value); cJSON_Delete(object); error = "Could not create component defaults."; return false; }
    }
    char* printed = cJSON_PrintUnformatted(object); cJSON_Delete(object);
    if (!printed) { error = "Could not create component defaults."; return false; }
    json = printed; cJSON_free(printed); error.clear(); return true;
}
