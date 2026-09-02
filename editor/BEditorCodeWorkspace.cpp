#include "BEditorCodeWorkspace.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;
namespace
{
bool Supported(const fs::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".c" || extension == ".h" || extension == ".cpp" || extension == ".hpp" ||
        extension == ".json" || extension == ".txt" || extension == ".cmake" || path.filename() == "CMakeLists.txt";
}
bool Read(const fs::path& path, std::string& text)
{
    std::ifstream stream(path, std::ios::binary); if (!stream) return false;
    text.assign(std::istreambuf_iterator<char>(stream), {}); return stream.good() || stream.eof();
}
}

bool BEditorCodeWorkspace::Resolve(const std::string& relativePath, fs::path& output, std::string& error) const
{
    fs::path relative(relativePath);
    if (root_.empty() || relative.empty() || relative.is_absolute()) { error = "A Project-relative file path is required."; return false; }
    fs::path normalized = relative.lexically_normal();
    if (normalized.empty() || *normalized.begin() == "..") { error = "File path escapes the Project root."; return false; }
    output = root_ / normalized; error.clear(); return true;
}

bool BEditorCodeWorkspace::OpenProject(const fs::path& root, std::string& error)
{
    std::error_code ec; fs::path absolute = fs::weakly_canonical(root, ec);
    if (ec || !fs::is_directory(absolute)) { error = "Project root is unavailable."; return false; }
    root_ = absolute; documents_.clear(); return RefreshTree(error);
}

bool BEditorCodeWorkspace::RefreshTree(std::string& error)
{
    std::vector<std::string> found; std::error_code ec;
    for (fs::recursive_directory_iterator it(root_, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec))
    {
        if (it->is_directory() && (it->path().filename() == "build" || it->path().filename().string().rfind("build-", 0) == 0)) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file() && Supported(it->path())) found.push_back(fs::relative(it->path(), root_).generic_string());
    }
    if (ec) { error = "Could not enumerate Project files: " + ec.message(); return false; }
    std::sort(found.begin(), found.end()); files_ = std::move(found); error.clear(); return true;
}

bool BEditorCodeWorkspace::OpenFile(const std::string& relativePath, std::string& error)
{
    for (const auto& item : documents_) if (item.relativePath == relativePath) { error.clear(); return true; }
    fs::path path; if (!Resolve(relativePath, path, error) || !Supported(path) || !fs::is_regular_file(path)) { if (error.empty()) error = "File is unavailable or unsupported."; return false; }
    std::string text; if (!Read(path, text)) { error = "Could not read Project file."; return false; }
    if (text.size() > 1024 * 1024) { error = "Built-in editor files are limited to 1 MiB."; return false; }
    documents_.push_back({relativePath, std::move(text), fs::last_write_time(path), false, false}); error.clear(); return true;
}

bool BEditorCodeWorkspace::SetText(std::size_t tab, const std::string& text, std::string& error)
{
    if (tab >= documents_.size() || text.size() > 1024 * 1024) { error = "Editor tab is unavailable or exceeds 1 MiB."; return false; }
    documents_[tab].text = text; documents_[tab].dirty = true; error.clear(); return true;
}

bool BEditorCodeWorkspace::Save(std::size_t tab, std::string& error)
{
    if (tab >= documents_.size()) { error = "Editor tab is unavailable."; return false; }
    auto& document = documents_[tab]; if (document.externalConflict) { error = "File changed externally; reload or resolve it before saving."; return false; }
    fs::path path; if (!Resolve(document.relativePath, path, error)) return false;
    fs::path temporary = path; temporary += ".tmp"; std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream.write(document.text.data(), static_cast<std::streamsize>(document.text.size())); stream.close();
    if (!stream) { error = "Could not write complete Project file."; return false; }
    std::error_code ec; fs::remove(path, ec); ec.clear(); fs::rename(temporary, path, ec);
    if (ec) { fs::remove(temporary); error = "Could not replace Project file: " + ec.message(); return false; }
    document.diskTime = fs::last_write_time(path); document.dirty = false; error.clear(); return true;
}

bool BEditorCodeWorkspace::SaveAll(std::string& error) { for (std::size_t i = 0; i < documents_.size(); ++i) if (documents_[i].dirty && !Save(i, error)) return false; error.clear(); return true; }
bool BEditorCodeWorkspace::PollExternalChanges(std::string& error)
{
    for (auto& document : documents_) { fs::path path; if (!Resolve(document.relativePath, path, error)) return false; std::error_code ec; auto time = fs::last_write_time(path, ec); if (ec) { error = "An open Project file was removed externally."; return false; } if (time != document.diskTime) { if (document.dirty) document.externalConflict = true; else { if (!Read(path, document.text)) { error = "Could not refresh externally changed file."; return false; } document.diskTime = time; } } }
    error.clear(); return true;
}
bool BEditorCodeWorkspace::CreateFile(const std::string& relativePath, std::string& error) { fs::path path; if (!Resolve(relativePath, path, error) || !Supported(path) || fs::exists(path)) { if (error.empty()) error = "File already exists or is unsupported."; return false; } std::error_code ec; fs::create_directories(path.parent_path(), ec); std::ofstream stream(path); if (ec || !stream) { error = "Could not create Project file."; return false; } stream.close(); return RefreshTree(error); }
bool BEditorCodeWorkspace::RenameFile(const std::string& oldPath, const std::string& newPath, std::string& error) { fs::path oldFile, newFile; if (!Resolve(oldPath, oldFile, error) || !Resolve(newPath, newFile, error) || !Supported(newFile) || fs::exists(newFile)) return false; for (const auto& d : documents_) if (d.relativePath == oldPath && d.dirty) { error = "Save the file before renaming it."; return false; } std::error_code ec; fs::rename(oldFile, newFile, ec); if (ec) { error = "Could not rename Project file: " + ec.message(); return false; } for (auto& d : documents_) if (d.relativePath == oldPath) d.relativePath = newPath; return RefreshTree(error); }
bool BEditorCodeWorkspace::DeleteFile(const std::string& relativePath, std::string& error) { fs::path path; if (!Resolve(relativePath, path, error)) return false; for (const auto& d : documents_) if (d.relativePath == relativePath) { error = "Close the file before deleting it."; return false; } std::error_code ec; if (!fs::remove(path, ec) || ec) { error = "Could not delete Project file."; return false; } return RefreshTree(error); }
bool BEditorCodeWorkspace::HasDirtyDocuments() const { for (const auto& item : documents_) if (item.dirty) return true; return false; }
