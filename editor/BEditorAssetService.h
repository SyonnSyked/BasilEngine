#ifndef BASIL_EDITOR_ASSET_SERVICE_H
#define BASIL_EDITOR_ASSET_SERVICE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

enum class BEditorAssetKind { TextSprite, Data, Font, Audio };

struct BEditorAssetRecord
{
    std::string id;
    std::string relativePath;
    BEditorAssetKind kind = BEditorAssetKind::Data;
    std::uintmax_t size = 0;
    std::uint64_t contentHash = 0;
    std::filesystem::file_time_type writeTime{};
};

struct BEditorAssetMove
{
    std::string oldPath;
    std::string newPath;
};

class BEditorAssetService
{
public:
    bool Open(const std::filesystem::path& projectRoot, std::string& error);
    bool Refresh(std::vector<BEditorAssetMove>& moves, std::string& error);
    bool Rename(const std::string& relativePath, const std::string& newName, std::vector<BEditorAssetMove>& moves, std::string& error);
    void Reset();
    const std::vector<BEditorAssetRecord>& Records() const;
    std::uint64_t Revision() const;
    static const char* KindName(BEditorAssetKind kind);

private:
    bool LoadRegistry(std::string& error);
    bool SaveRegistry(std::string& error) const;
    std::filesystem::path root_;
    std::vector<BEditorAssetRecord> records_;
    std::uint64_t revision_ = 1;
};

#endif
