#ifndef BASIL_EDITOR_CODE_WORKSPACE_H
#define BASIL_EDITOR_CODE_WORKSPACE_H

#include <filesystem>
#include <string>
#include <vector>

struct BEditorCodeDocument
{
    std::string relativePath;
    std::string text;
    std::filesystem::file_time_type diskTime{};
    bool dirty = false;
    bool externalConflict = false;
};

class BEditorCodeWorkspace
{
public:
    bool OpenProject(const std::filesystem::path& root, std::string& error);
    bool RefreshTree(std::string& error);
    bool OpenFile(const std::string& relativePath, std::string& error);
    bool SetText(std::size_t tab, const std::string& text, std::string& error);
    bool Save(std::size_t tab, std::string& error);
    bool SaveAll(std::string& error);
    bool Reload(std::size_t tab, std::string& error);
    bool Close(std::size_t tab, bool discardChanges, std::string& error);
    bool PollExternalChanges(std::string& error);
    bool CreateFile(const std::string& relativePath, std::string& error);
    bool RenameFile(const std::string& oldPath, const std::string& newPath, std::string& error);
    bool DeleteFile(const std::string& relativePath, std::string& error);
    bool HasDirtyDocuments() const;
    const std::filesystem::path& Root() const { return root_; }
    const std::vector<std::string>& Files() const { return files_; }
    std::vector<BEditorCodeDocument>& Documents() { return documents_; }
    const std::vector<BEditorCodeDocument>& Documents() const { return documents_; }

private:
    bool Resolve(const std::string& relativePath, std::filesystem::path& output, std::string& error) const;
    std::filesystem::path root_;
    std::vector<std::string> files_;
    std::vector<BEditorCodeDocument> documents_;
};

#endif
