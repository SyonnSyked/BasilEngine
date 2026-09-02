#include "BEditorAssetService.h"

#include "cJSON.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace fs = std::filesystem;
namespace
{
constexpr int SchemaVersion = 1;
constexpr std::uintmax_t MaximumAssetSize = 256u * 1024u * 1024u;

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool Classify(const fs::path& path, BEditorAssetKind& kind)
{
    std::string extension = Lower(path.extension().string());
    if (extension == ".txt") kind = BEditorAssetKind::TextSprite;
    else if (extension == ".json") kind = BEditorAssetKind::Data;
    else if (extension == ".ttf" || extension == ".otf") kind = BEditorAssetKind::Font;
    else if (extension == ".wav" || extension == ".ogg") kind = BEditorAssetKind::Audio;
    else return false;
    return true;
}

std::uint64_t HashStream(std::ifstream& stream)
{
    std::uint64_t hash = 1469598103934665603ull;
    char buffer[64 * 1024];
    while (stream)
    {
        stream.read(buffer, sizeof(buffer));
        for (std::streamsize i = 0; i < stream.gcount(); ++i)
        {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

bool Inspect(const fs::path& absolute, BEditorAssetRecord& record, std::string& error)
{
    std::error_code fileError;
    record.size = fs::file_size(absolute, fileError);
    if (fileError || record.size > MaximumAssetSize)
    {
        error = fileError ? "Could not measure asset: " + fileError.message() : "Asset exceeds the 256 MiB alpha limit.";
        return false;
    }
    std::ifstream stream(absolute, std::ios::binary);
    if (!stream) { error = "Could not read asset: " + absolute.string(); return false; }
    record.contentHash = HashStream(stream);
    record.writeTime = fs::last_write_time(absolute, fileError);
    if (fileError) { error = "Could not inspect asset timestamp: " + fileError.message(); return false; }
    return true;
}

bool IsContained(const fs::path& root, const fs::path& candidate)
{
    std::error_code error;
    fs::path canonicalRoot = fs::weakly_canonical(root, error);
    if (error) return false;
    fs::path canonicalCandidate = fs::weakly_canonical(candidate, error);
    if (error) return false;
    fs::path relative = canonicalCandidate.lexically_relative(canonicalRoot);
    return !relative.empty() && *relative.begin() != "..";
}

std::string MakeId(const std::string& path, const std::unordered_set<std::string>& used)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char value : path) { hash ^= value; hash *= 1099511628211ull; }
    char candidate[48];
    for (unsigned int suffix = 0; ; ++suffix)
    {
        std::snprintf(candidate, sizeof(candidate), suffix == 0 ? "asset-%016llx" : "asset-%016llx-%u",
            static_cast<unsigned long long>(hash), suffix);
        if (used.find(candidate) == used.end()) return candidate;
    }
}

const char* KindToken(BEditorAssetKind kind)
{
    switch (kind) {
        case BEditorAssetKind::TextSprite: return "text-sprite";
        case BEditorAssetKind::Data: return "data";
        case BEditorAssetKind::Font: return "font";
        case BEditorAssetKind::Audio: return "audio";
    }
    return "data";
}

bool ParseKind(const char* value, BEditorAssetKind& kind)
{
    if (!value) return false;
    if (std::string(value) == "text-sprite") kind = BEditorAssetKind::TextSprite;
    else if (std::string(value) == "data") kind = BEditorAssetKind::Data;
    else if (std::string(value) == "font") kind = BEditorAssetKind::Font;
    else if (std::string(value) == "audio") kind = BEditorAssetKind::Audio;
    else return false;
    return true;
}
}

bool BEditorAssetService::Open(const fs::path& projectRoot, std::string& error)
{
    Reset();
    std::error_code pathError;
    root_ = fs::absolute(projectRoot, pathError).lexically_normal();
    if (pathError || !fs::is_directory(root_ / "assets", pathError))
    {
        error = "Project asset directory is unavailable.";
        return false;
    }
    if (!LoadRegistry(error)) return false;
    std::vector<BEditorAssetMove> ignored;
    return Refresh(ignored, error);
}

void BEditorAssetService::Reset() { root_.clear(); records_.clear(); ++revision_; }
const std::vector<BEditorAssetRecord>& BEditorAssetService::Records() const { return records_; }
std::uint64_t BEditorAssetService::Revision() const { return revision_; }
const char* BEditorAssetService::KindName(BEditorAssetKind kind) { return KindToken(kind); }

bool BEditorAssetService::LoadRegistry(std::string& error)
{
    fs::path path = root_ / ".basil" / "assets.json";
    if (!fs::exists(path)) { error.clear(); return true; }
    std::ifstream stream(path, std::ios::binary);
    std::string contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    cJSON* root = cJSON_Parse(contents.c_str());
    cJSON* schema = root ? cJSON_GetObjectItemCaseSensitive(root, "schemaVersion") : nullptr;
    cJSON* assets = root ? cJSON_GetObjectItemCaseSensitive(root, "assets") : nullptr;
    if (!cJSON_IsObject(root) || !cJSON_IsNumber(schema) || schema->valueint != SchemaVersion || !cJSON_IsArray(assets))
    { cJSON_Delete(root); error = "Asset registry contains invalid or unsupported JSON."; return false; }
    std::unordered_set<std::string> ids, paths;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, assets)
    {
        cJSON* id = cJSON_GetObjectItemCaseSensitive(item, "id");
        cJSON* relative = cJSON_GetObjectItemCaseSensitive(item, "path");
        cJSON* type = cJSON_GetObjectItemCaseSensitive(item, "type");
        cJSON* size = cJSON_GetObjectItemCaseSensitive(item, "size");
        cJSON* hash = cJSON_GetObjectItemCaseSensitive(item, "hash");
        BEditorAssetRecord record;
        if (!cJSON_IsString(id) || !cJSON_IsString(relative) || !cJSON_IsString(type) || !cJSON_IsNumber(size) ||
            !cJSON_IsString(hash) || !ParseKind(type->valuestring, record.kind) || !ids.insert(id->valuestring).second ||
            !paths.insert(relative->valuestring).second)
        { cJSON_Delete(root); records_.clear(); error = "Asset registry contains an invalid or duplicate record."; return false; }
        record.id = id->valuestring; record.relativePath = relative->valuestring;
        record.size = static_cast<std::uintmax_t>(size->valuedouble);
        try { record.contentHash = std::stoull(hash->valuestring, nullptr, 16); }
        catch (...) { cJSON_Delete(root); records_.clear(); error = "Asset registry contains an invalid content hash."; return false; }
        records_.push_back(std::move(record));
    }
    cJSON_Delete(root); error.clear(); return true;
}

bool BEditorAssetService::SaveRegistry(std::string& error) const
{
    fs::path destination = root_ / ".basil" / "assets.json";
    std::error_code fileError; fs::create_directories(destination.parent_path(), fileError);
    if (fileError) { error = "Could not create asset registry directory: " + fileError.message(); return false; }
    cJSON* root = cJSON_CreateObject(); cJSON* assets = cJSON_CreateArray();
    if (!root || !assets) { cJSON_Delete(root); cJSON_Delete(assets); error = "Out of memory while writing asset registry."; return false; }
    cJSON_AddNumberToObject(root, "schemaVersion", SchemaVersion); cJSON_AddItemToObject(root, "assets", assets);
    for (const auto& record : records_)
    {
        cJSON* item = cJSON_CreateObject(); char hash[24];
        std::snprintf(hash, sizeof(hash), "%016llx", static_cast<unsigned long long>(record.contentHash));
        cJSON_AddStringToObject(item, "id", record.id.c_str()); cJSON_AddStringToObject(item, "path", record.relativePath.c_str());
        cJSON_AddStringToObject(item, "type", KindToken(record.kind)); cJSON_AddNumberToObject(item, "size", static_cast<double>(record.size));
        cJSON_AddStringToObject(item, "hash", hash); cJSON_AddItemToArray(assets, item);
    }
    char* json = cJSON_Print(root); cJSON_Delete(root);
    if (!json) { error = "Could not serialize asset registry."; return false; }
    fs::path temporary = destination; temporary += ".tmp";
    FILE* file = std::fopen(temporary.string().c_str(), "wb");
    if (!file) { cJSON_free(json); error = "Could not open asset registry for writing."; return false; }
    size_t length = std::char_traits<char>::length(json); bool written = std::fwrite(json, 1, length, file) == length && std::fwrite("\n", 1, 1, file) == 1;
    bool closed = std::fclose(file) == 0; cJSON_free(json);
    if (!written || !closed) { fs::remove(temporary, fileError); error = "Could not write complete asset registry."; return false; }
    fs::path backup = destination; backup += ".bak";
    if (fs::exists(destination)) { fs::remove(backup, fileError); fileError.clear(); fs::rename(destination, backup, fileError); }
    if (!fileError) fs::rename(temporary, destination, fileError);
    if (fileError)
    {
        std::error_code ignored;
        if (fs::exists(backup, ignored) && !fs::exists(destination, ignored)) fs::rename(backup, destination, ignored);
        fs::remove(temporary, ignored);
        error = "Could not replace asset registry: " + fileError.message(); return false;
    }
    error.clear(); return true;
}

bool BEditorAssetService::Refresh(std::vector<BEditorAssetMove>& moves, std::string& error)
{
    moves.clear();
    if (root_.empty()) { error = "Asset service has no open Project."; return false; }
    std::vector<BEditorAssetRecord> discovered;
    std::error_code scanError;
    for (fs::recursive_directory_iterator it(root_ / "assets", scanError), end; !scanError && it != end; it.increment(scanError))
    {
        if (!it->is_regular_file(scanError)) continue;
        BEditorAssetRecord record;
        if (!Classify(it->path(), record.kind)) continue;
        if (!IsContained(root_ / "assets", it->path())) { error = "Resolved asset escapes the Project asset directory."; return false; }
        record.relativePath = it->path().lexically_relative(root_).generic_string();
        if (!Inspect(it->path(), record, error)) return false;
        discovered.push_back(std::move(record));
    }
    if (scanError) { error = "Could not scan Project assets: " + scanError.message(); return false; }
    std::unordered_set<std::string> usedIds;
    for (const auto& record : records_) usedIds.insert(record.id);
    std::vector<bool> oldUsed(records_.size(), false);
    for (auto& fresh : discovered)
    {
        auto pathMatch = std::find_if(records_.begin(), records_.end(), [&](const auto& old) { return old.relativePath == fresh.relativePath; });
        if (pathMatch != records_.end()) { size_t index = static_cast<size_t>(pathMatch - records_.begin()); oldUsed[index] = true; fresh.id = pathMatch->id; continue; }
        size_t match = records_.size();
        for (size_t i = 0; i < records_.size(); ++i)
            if (!oldUsed[i] && records_[i].size == fresh.size && records_[i].contentHash == fresh.contentHash) { if (match != records_.size()) { match = records_.size(); break; } match = i; }
        if (match != records_.size()) { oldUsed[match] = true; fresh.id = records_[match].id; moves.push_back({records_[match].relativePath, fresh.relativePath}); }
        else { fresh.id = MakeId(fresh.relativePath, usedIds); usedIds.insert(fresh.id); }
    }
    std::sort(discovered.begin(), discovered.end(), [](const auto& a, const auto& b) { return a.relativePath < b.relativePath; });
    bool changed = discovered.size() != records_.size() || !std::equal(discovered.begin(), discovered.end(), records_.begin(), [](const auto& a, const auto& b) {
        return a.id == b.id && a.relativePath == b.relativePath && a.size == b.size && a.contentHash == b.contentHash && a.kind == b.kind; });
    records_ = std::move(discovered);
    if (changed) { if (!SaveRegistry(error)) return false; ++revision_; }
    error.clear(); return true;
}

bool BEditorAssetService::Rename(const std::string& relativePath, const std::string& newName, std::vector<BEditorAssetMove>& moves, std::string& error)
{
    fs::path oldRelative(relativePath);
    if (newName.empty() || fs::path(newName).filename() != fs::path(newName) || newName == "." || newName == "..")
    { error = "Asset name must be one filename."; return false; }
    fs::path destination = oldRelative.parent_path() / newName;
    BEditorAssetKind oldKind, newKind;
    if (!Classify(oldRelative, oldKind) || !Classify(destination, newKind) || oldKind != newKind)
    { error = "Asset rename must retain its supported file type."; return false; }
    if (!IsContained(root_ / "assets", root_ / oldRelative) || fs::exists(root_ / destination))
    { error = "Asset rename escapes the asset root or would overwrite an existing file."; return false; }
    std::error_code fileError;
    fs::rename(root_ / oldRelative, root_ / destination, fileError);
    if (fileError) { error = "Could not rename asset: " + fileError.message(); return false; }
    return Refresh(moves, error);
}
