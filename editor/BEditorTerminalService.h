#ifndef BASIL_EDITOR_TERMINAL_SERVICE_H
#define BASIL_EDITOR_TERMINAL_SERVICE_H

#include <filesystem>
#include <memory>
#include <string>

class BEditorTerminalService
{
public:
    BEditorTerminalService();
    ~BEditorTerminalService();
    BEditorTerminalService(const BEditorTerminalService&) = delete;
    BEditorTerminalService& operator=(const BEditorTerminalService&) = delete;
    bool Start(const std::string& shell, const std::filesystem::path& projectRoot, std::string& error);
    bool Send(const std::string& command, std::string& error);
    bool Restart(std::string& error);
    void Update();
    void Clear();
    void Stop();
    bool IsRunning() const;
    const std::string& Output() const;
private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

#endif
