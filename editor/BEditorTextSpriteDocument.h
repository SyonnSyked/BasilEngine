#ifndef BASIL_EDITOR_TEXT_SPRITE_DOCUMENT_H
#define BASIL_EDITOR_TEXT_SPRITE_DOCUMENT_H

#include "BTextSprite.h"

#include <filesystem>
#include <string>
#include <vector>

class BEditorTextSpriteDocument
{
public:
    BEditorTextSpriteDocument();
    ~BEditorTextSpriteDocument();
    bool Open(const std::filesystem::path& projectRoot, const std::string& relativePath, std::string& error);
    bool Create(const std::filesystem::path& projectRoot, const std::string& relativePath, std::string& error);
    bool Save(std::string& error);
    bool Reload(std::string& error);
    void Reset();
    void MarkEdited();
    bool RefreshPreview(std::string& error);
    bool HasExternalChange() const;
    void AcceptExternalChange();
    bool IsOpen() const;
    bool IsDirty() const;
    char* Buffer();
    std::size_t Capacity() const;
    const std::string& RelativePath() const;
    const BTextSprite& Preview() const;

private:
    bool Resolve(const std::filesystem::path& root, const std::string& relative, std::filesystem::path& output, std::string& error) const;
    std::filesystem::path root_;
    std::filesystem::path path_;
    std::string relativePath_;
    std::vector<char> buffer_;
    BTextSprite preview_{};
    std::filesystem::file_time_type loadedWriteTime_{};
    bool open_ = false;
    bool dirty_ = false;
};

#endif
