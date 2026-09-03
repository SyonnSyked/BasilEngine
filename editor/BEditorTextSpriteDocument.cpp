#include "BEditorTextSpriteDocument.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

BEditorTextSpriteDocument::BEditorTextSpriteDocument() : buffer_(BTEXT_SPRITE_SOURCE_MAX + 1, '\0') { BTextSprite_Init(&preview_); }
BEditorTextSpriteDocument::~BEditorTextSpriteDocument() { BTextSprite_Destroy(&preview_); }

bool BEditorTextSpriteDocument::Resolve(const fs::path& root, const std::string& relative, fs::path& output, std::string& error) const
{
    fs::path normalizedRoot = fs::absolute(root).lexically_normal();
    fs::path candidate = (normalizedRoot / relative).lexically_normal();
    fs::path rel = candidate.lexically_relative(normalizedRoot);
    if (relative.empty() || fs::path(relative).is_absolute() || rel.empty() || *rel.begin() == ".." ||
        rel.begin()->string() != "assets" || candidate.extension() != ".txt")
    { error = "Text Sprite must be a .txt file inside the Project assets directory."; return false; }
    std::error_code canonicalError;
    fs::path canonicalAssets = fs::weakly_canonical(normalizedRoot / "assets", canonicalError);
    fs::path canonicalCandidate = fs::weakly_canonical(candidate, canonicalError);
    fs::path contained = canonicalCandidate.lexically_relative(canonicalAssets);
    if (canonicalError || contained.empty() || *contained.begin() == "..")
    { error = "Resolved Text Sprite escapes the Project assets directory."; return false; }
    output = candidate; return true;
}

bool BEditorTextSpriteDocument::Open(const fs::path& projectRoot, const std::string& relativePath, std::string& error)
{
    fs::path candidate;
    if (!Resolve(projectRoot, relativePath, candidate, error)) return false;
    std::ifstream stream(candidate, std::ios::binary);
    if (!stream) { error = "Could not open Text Sprite."; return false; }
    std::string contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (contents.size() > BTEXT_SPRITE_SOURCE_MAX) { error = "Text Sprite exceeds its source-size limit."; return false; }
    BDiagnosticList diagnostics{}; BTextSprite decoded; BTextSprite_Init(&decoded);
    if (!BTextSprite_Decode(contents.data(), contents.size(), relativePath.c_str(), &decoded, &diagnostics))
    { const BDiagnostic* item = BDiagnosticList_FirstError(&diagnostics); error = item ? item->message : "Text Sprite is invalid."; return false; }
    std::fill(buffer_.begin(), buffer_.end(), '\0'); std::memcpy(buffer_.data(), contents.data(), contents.size());
    BTextSprite_Swap(&preview_, &decoded); BTextSprite_Destroy(&decoded);
    root_ = fs::absolute(projectRoot).lexically_normal(); path_ = candidate; relativePath_ = relativePath;
    std::error_code timeError; loadedWriteTime_ = fs::last_write_time(path_, timeError);
    open_ = true; dirty_ = false; error.clear(); return true;
}

bool BEditorTextSpriteDocument::Create(const fs::path& projectRoot, const std::string& relativePath, std::string& error)
{
    fs::path candidate;
    if (!Resolve(projectRoot, relativePath, candidate, error)) return false;
    if (fs::exists(candidate)) { error = "A Text Sprite already exists at that path."; return false; }
    std::error_code directoryError; fs::create_directories(candidate.parent_path(), directoryError);
    if (directoryError) { error = "Could not create Text Sprite directory: " + directoryError.message(); return false; }
    root_ = fs::absolute(projectRoot).lexically_normal(); path_ = candidate; relativePath_ = relativePath;
    std::fill(buffer_.begin(), buffer_.end(), '\0'); std::memcpy(buffer_.data(), "@\n", 2);
    open_ = true; dirty_ = true;
    return Save(error);
}

bool BEditorTextSpriteDocument::RefreshPreview(std::string& error)
{
    if (!open_) { error = "No Text Sprite is open."; return false; }
    BDiagnosticList diagnostics{};
    if (!BTextSprite_Decode(buffer_.data(), std::strlen(buffer_.data()), relativePath_.c_str(), &preview_, &diagnostics))
    { const BDiagnostic* item = BDiagnosticList_FirstError(&diagnostics); error = item ? item->message : "Text Sprite is invalid."; return false; }
    error.clear(); return true;
}

void BEditorTextSpriteDocument::MarkEdited() { if (open_) dirty_ = true; }

bool BEditorTextSpriteDocument::Save(std::string& error)
{
    if (!RefreshPreview(error)) return false;
    fs::path temporary = path_; temporary += ".tmp";
    FILE* file = std::fopen(temporary.string().c_str(), "wb");
    if (!file) { error = "Could not open Text Sprite for writing."; return false; }
    size_t length = std::strlen(buffer_.data()); bool written = std::fwrite(buffer_.data(), 1, length, file) == length;
    bool closed = std::fclose(file) == 0; std::error_code fileError;
    if (!written || !closed) { fs::remove(temporary, fileError); error = "Could not write complete Text Sprite."; return false; }
    fs::path backup = path_; backup += ".bak";
    if (fs::exists(path_)) { fs::remove(backup, fileError); fileError.clear(); fs::rename(path_, backup, fileError); }
    if (!fileError) fs::rename(temporary, path_, fileError);
    if (fileError)
    {
        std::error_code ignored;
        if (fs::exists(backup, ignored) && !fs::exists(path_, ignored)) fs::rename(backup, path_, ignored);
        fs::remove(temporary, ignored);
        error = "Could not replace Text Sprite: " + fileError.message(); return false;
    }
    loadedWriteTime_ = fs::last_write_time(path_, fileError); dirty_ = false; error.clear(); return true;
}

bool BEditorTextSpriteDocument::Reload(std::string& error) { return Open(root_, relativePath_, error); }
void BEditorTextSpriteDocument::Reset() { root_.clear(); path_.clear(); relativePath_.clear(); std::fill(buffer_.begin(), buffer_.end(), '\0'); BTextSprite_Destroy(&preview_); open_ = dirty_ = false; }
bool BEditorTextSpriteDocument::HasExternalChange() const { if (!open_) return false; std::error_code error; auto current = fs::last_write_time(path_, error); return !error && current != loadedWriteTime_; }
void BEditorTextSpriteDocument::AcceptExternalChange() { if (!open_) return; std::error_code error; loadedWriteTime_ = fs::last_write_time(path_, error); }
bool BEditorTextSpriteDocument::IsOpen() const { return open_; }
bool BEditorTextSpriteDocument::IsDirty() const { return dirty_; }
char* BEditorTextSpriteDocument::Buffer() { return buffer_.data(); }
std::size_t BEditorTextSpriteDocument::Capacity() const { return buffer_.size(); }
const std::string& BEditorTextSpriteDocument::RelativePath() const { return relativePath_; }
const BTextSprite& BEditorTextSpriteDocument::Preview() const { return preview_; }
