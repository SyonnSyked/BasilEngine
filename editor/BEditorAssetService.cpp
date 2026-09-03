#include "BEditorAssetService.h"

#include "../engine/project/BAssetRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace fs = std::filesystem;
namespace {

std::string RegistryError(const BDiagnosticList &diagnostics, const char *fallback)
{
    const BDiagnostic *diagnostic = BDiagnosticList_FirstError(&diagnostics);

    if (diagnostic != nullptr && diagnostic->message[0] != '\0') {
        return diagnostic->message;
    }

    return fallback;
}

BEditorAssetKind FromSharedKind(BAssetKind kind)
{
    switch (kind) {
        case BASSET_KIND_TEXT_SPRITE:
            return BEditorAssetKind::TextSprite;

        case BASSET_KIND_DATA:
            return BEditorAssetKind::Data;

        case BASSET_KIND_FONT:
            return BEditorAssetKind::Font;

        case BASSET_KIND_AUDIO:
            return BEditorAssetKind::Audio;
    }

    return BEditorAssetKind::Data;
}

BAssetKind ToSharedKind(BEditorAssetKind kind)
{
    switch (kind) {
        case BEditorAssetKind::TextSprite:
            return BASSET_KIND_TEXT_SPRITE;

        case BEditorAssetKind::Data:
            return BASSET_KIND_DATA;

        case BEditorAssetKind::Font:
            return BASSET_KIND_FONT;

        case BEditorAssetKind::Audio:
            return BASSET_KIND_AUDIO;
    }

    return BASSET_KIND_DATA;
}

bool BuildSharedRegistry(const std::vector<BEditorAssetRecord> &records, BAssetRegistry &registry,
                         std::string &error)
{
    std::vector<BAssetRecord> shared(records.size());

    for (size_t i = 0; i < records.size(); ++i) {
        const BEditorAssetRecord &editor = records[i];

        BAssetRecord &record = shared[i];

        if (editor.id.size() >= sizeof(record.id) ||
            editor.relativePath.size() >= sizeof(record.path)) {
            error = "Asset registry record exceeds shared Project limits.";

            return false;
        }

        std::snprintf(record.id, sizeof(record.id), "%s", editor.id.c_str());

        std::snprintf(record.path, sizeof(record.path), "%s", editor.relativePath.c_str());

        record.kind = ToSharedKind(editor.kind);

        record.size = editor.size;

        record.contentHash = editor.contentHash;
    }

    BDiagnosticList diagnostics;

    if (!BAssetRegistry_Assign(&registry, shared.data(), shared.size(), &diagnostics)) {
        error = RegistryError(diagnostics, "Could not prepare shared asset registry.");

        return false;
    }

    return true;
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool Classify(const fs::path &path, BEditorAssetKind &kind)
{
    std::string extension = Lower(path.extension().string());
    if (extension == ".txt")
        kind = BEditorAssetKind::TextSprite;
    else if (extension == ".json")
        kind = BEditorAssetKind::Data;
    else if (extension == ".ttf" || extension == ".otf")
        kind = BEditorAssetKind::Font;
    else if (extension == ".wav" || extension == ".ogg")
        kind = BEditorAssetKind::Audio;
    else
        return false;
    return true;
}

std::uint64_t HashStream(std::ifstream &stream)
{
    std::uint64_t hash = 1469598103934665603ull;
    char buffer[64 * 1024];
    while (stream) {
        stream.read(buffer, sizeof(buffer));
        for (std::streamsize i = 0; i < stream.gcount(); ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

bool Inspect(const fs::path &absolute, BEditorAssetRecord &record, std::string &error)
{
    std::error_code fileError;
    record.size = fs::file_size(absolute, fileError);
    if (fileError || record.size > BASSET_FILE_MAX) {
        error = fileError ? "Could not measure asset: " + fileError.message()
                          : "Asset exceeds the 256 MiB alpha limit.";
        return false;
    }
    std::ifstream stream(absolute, std::ios::binary);
    if (!stream) {
        error = "Could not read asset: " + absolute.string();
        return false;
    }
    record.contentHash = HashStream(stream);
    record.writeTime = fs::last_write_time(absolute, fileError);
    if (fileError) {
        error = "Could not inspect asset timestamp: " + fileError.message();
        return false;
    }
    return true;
}

bool IsContained(const fs::path &root, const fs::path &candidate)
{
    std::error_code error;
    fs::path canonicalRoot = fs::weakly_canonical(root, error);
    if (error)
        return false;
    fs::path canonicalCandidate = fs::weakly_canonical(candidate, error);
    if (error)
        return false;
    fs::path relative = canonicalCandidate.lexically_relative(canonicalRoot);
    return !relative.empty() && *relative.begin() != "..";
}

} // namespace

bool BEditorAssetService::Open(const fs::path &projectRoot, std::string &error)
{
    Reset();
    std::error_code pathError;
    root_ = fs::absolute(projectRoot, pathError).lexically_normal();
    if (pathError || !fs::is_directory(root_ / "assets", pathError)) {
        error = "Project asset directory is unavailable.";
        return false;
    }
    if (!LoadRegistry(error))
        return false;
    std::vector<BEditorAssetMove> ignored;
    return Refresh(ignored, error);
}

void BEditorAssetService::Reset()
{
    root_.clear();
    records_.clear();
    ++revision_;
}
const std::vector<BEditorAssetRecord> &BEditorAssetService::Records() const
{
    return records_;
}
std::uint64_t BEditorAssetService::Revision() const
{
    return revision_;
}
const char *BEditorAssetService::KindName(BEditorAssetKind kind)
{
    return BAssetKind_ToString(ToSharedKind(kind));
}

bool BEditorAssetService::LoadRegistry(std::string &error)
{
    fs::path path = root_ / ".basil" / "assets.json";

    if (!fs::exists(path)) {
        error.clear();
        return true;
    }

    BAssetRegistry registry;
    BAssetRegistry_Init(&registry);

    BDiagnosticList diagnostics;

    if (!BAssetRegistry_Load(path.string().c_str(), &registry, &diagnostics)) {
        error = RegistryError(diagnostics, "Could not load asset registry.");

        BAssetRegistry_Destroy(&registry);

        return false;
    }

    std::vector<BEditorAssetRecord> loaded;
    loaded.reserve(registry.count);

    for (size_t i = 0; i < registry.count; ++i) {
        const BAssetRecord &shared = registry.records[i];

        BEditorAssetRecord record;

        record.id = shared.id;
        record.relativePath = shared.path;
        record.kind = FromSharedKind(shared.kind);
        record.size = shared.size;
        record.contentHash = shared.contentHash;

        loaded.push_back(std::move(record));
    }

    BAssetRegistry_Destroy(&registry);

    records_ = std::move(loaded);

    error.clear();
    return true;
}

bool BEditorAssetService::SaveRegistry(std::string &error) const
{
    fs::path destination = root_ / ".basil" / "assets.json";

    std::error_code directoryError;

    fs::create_directories(destination.parent_path(), directoryError);

    if (directoryError) {
        error = "Could not create asset registry directory: " + directoryError.message();

        return false;
    }

    BAssetRegistry registry;
    BAssetRegistry_Init(&registry);

    if (!BuildSharedRegistry(records_, registry, error)) {
        BAssetRegistry_Destroy(&registry);
        return false;
    }

    BDiagnosticList diagnostics;

    if (!BAssetRegistry_Save(&registry, destination.string().c_str(), &diagnostics)) {
        error = RegistryError(diagnostics, "Could not save asset registry.");

        BAssetRegistry_Destroy(&registry);

        return false;
    }

    BAssetRegistry_Destroy(&registry);

    error.clear();
    return true;
}

bool BEditorAssetService::Refresh(std::vector<BEditorAssetMove> &moves, std::string &error)
{
    moves.clear();
    if (root_.empty()) {
        error = "Asset service has no open Project.";
        return false;
    }
    std::vector<BEditorAssetRecord> discovered;
    std::error_code scanError;
    for (fs::recursive_directory_iterator it(root_ / "assets", scanError), end;
         !scanError && it != end; it.increment(scanError)) {
        if (!it->is_regular_file(scanError))
            continue;
        BEditorAssetRecord record;
        if (!Classify(it->path(), record.kind))
            continue;
        if (!IsContained(root_ / "assets", it->path())) {
            error = "Resolved asset escapes the Project asset directory.";
            return false;
        }
        record.relativePath = it->path().lexically_relative(root_).generic_string();
        if (!Inspect(it->path(), record, error))
            return false;
        discovered.push_back(std::move(record));
    }
    if (scanError) {
        error = "Could not scan Project assets: " + scanError.message();
        return false;
    }

    BAssetRegistry previousRegistry;
    BAssetRegistry_Init(&previousRegistry);

    if (!BuildSharedRegistry(records_, previousRegistry, error)) {
        BAssetRegistry_Destroy(&previousRegistry);

        return false;
    }

    std::vector<BAssetObservation> observations(discovered.size());

    for (size_t i = 0; i < discovered.size(); ++i) {
        const BEditorAssetRecord &fresh = discovered[i];

        BAssetObservation &observation = observations[i];

        if (fresh.relativePath.size() >= sizeof(observation.path)) {
            BAssetRegistry_Destroy(&previousRegistry);

            error = "Observed asset path exceeds shared Project limits.";

            return false;
        }

        std::snprintf(observation.path, sizeof(observation.path), "%s", fresh.relativePath.c_str());

        observation.kind = ToSharedKind(fresh.kind);

        observation.size = fresh.size;

        observation.contentHash = fresh.contentHash;
    }

    BAssetRegistry reconciled;
    BAssetRegistry_Init(&reconciled);

    BDiagnosticList diagnostics;

    if (!BAssetRegistry_Reconcile(&previousRegistry, observations.data(), observations.size(),
                                  &reconciled, &diagnostics)) {
        error = RegistryError(diagnostics, "Could not reconcile asset identities.");

        BAssetRegistry_Destroy(&reconciled);

        BAssetRegistry_Destroy(&previousRegistry);

        return false;
    }

    std::unordered_map<std::string, std::string> oldPathById;

    for (const BEditorAssetRecord &old : records_) {
        oldPathById[old.id] = old.relativePath;
    }

    std::unordered_map<std::string, std::string> idByPath;

    for (size_t i = 0; i < reconciled.count; ++i) {
        const BAssetRecord &record = reconciled.records[i];

        idByPath[record.path] = record.id;

        auto old = oldPathById.find(record.id);

        if (old != oldPathById.end() && old->second != record.path) {
            moves.push_back({old->second, record.path});
        }
    }

    for (BEditorAssetRecord &fresh : discovered) {
        auto identity = idByPath.find(fresh.relativePath);

        if (identity == idByPath.end()) {
            BAssetRegistry_Destroy(&reconciled);

            BAssetRegistry_Destroy(&previousRegistry);

            error = "Shared asset registry did not return an identity for an observed asset.";

            return false;
        }

        fresh.id = identity->second;
    }

    BAssetRegistry_Destroy(&reconciled);

    BAssetRegistry_Destroy(&previousRegistry);

    std::sort(discovered.begin(), discovered.end(),
              [](const auto &a, const auto &b) { return a.relativePath < b.relativePath; });
    bool changed = discovered.size() != records_.size() ||
                   !std::equal(discovered.begin(), discovered.end(), records_.begin(),
                               [](const auto &a, const auto &b) {
                                   return a.id == b.id && a.relativePath == b.relativePath &&
                                          a.size == b.size && a.contentHash == b.contentHash &&
                                          a.kind == b.kind;
                               });
    if (changed) {
        std::vector<BEditorAssetRecord> previous = records_;

        records_ = discovered;

        if (!SaveRegistry(error)) {
            records_ = std::move(previous);
            return false;
        }

        ++revision_;
    }

    records_ = std::move(discovered);

    error.clear();
    return true;
}

bool BEditorAssetService::Rename(const std::string &relativePath, const std::string &newName,
                                 std::vector<BEditorAssetMove> &moves, std::string &error)
{
    fs::path oldRelative(relativePath);
    if (newName.empty() || fs::path(newName).filename() != fs::path(newName) || newName == "." ||
        newName == "..") {
        error = "Asset name must be one filename.";
        return false;
    }
    fs::path destination = oldRelative.parent_path() / newName;
    BEditorAssetKind oldKind, newKind;
    if (!Classify(oldRelative, oldKind) || !Classify(destination, newKind) || oldKind != newKind) {
        error = "Asset rename must retain its supported file type.";
        return false;
    }
    if (!IsContained(root_ / "assets", root_ / oldRelative) || fs::exists(root_ / destination)) {
        error = "Asset rename escapes the asset root or would overwrite an existing file.";
        return false;
    }
    std::error_code fileError;
    fs::rename(root_ / oldRelative, root_ / destination, fileError);
    if (fileError) {
        error = "Could not rename asset: " + fileError.message();
        return false;
    }
    return Refresh(moves, error);
}
