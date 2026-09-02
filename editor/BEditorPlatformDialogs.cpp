#include "BEditorPlatformDialogs.h"

#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

namespace
{
HWND closeWindow = nullptr;
WNDPROC originalWindowProcedure = nullptr;
bool closeRequested = false;

LRESULT CALLBACK EditorWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CLOSE) { closeRequested = true; return 0; }
    return CallWindowProcA(originalWindowProcedure, window, message, wParam, lParam);
}

bool FileDialog(bool save, const char* filter, const char* extension, std::filesystem::path& path, std::string& error)
{
    char buffer[32768]{};
    std::string initial = path.string();
    if (!initial.empty()) std::snprintf(buffer, sizeof(buffer), "%s", initial.c_str());
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = sizeof(buffer);
    dialog.lpstrDefExt = extension;
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    BOOL accepted = save ? GetSaveFileNameA(&dialog) : GetOpenFileNameA(&dialog);
    if (accepted) { path = buffer; error.clear(); return true; }
    DWORD code = CommDlgExtendedError();
    if (code != 0) { error = "Native file dialog failed with code " + std::to_string(code) + "."; }
    else error.clear();
    return false;
}
}

bool BEditorDialog_OpenProject(std::filesystem::path& path, std::string& error)
{
    return FileDialog(false, "BasilEngine Projects\0*.basilproject\0All Files\0*.*\0", "basilproject", path, error);
}
bool BEditorDialog_OpenUIConfig(std::filesystem::path& path, std::string& error)
{
    return FileDialog(false, "BasilEditor UI Configs\0*.basilui.json\0JSON Files\0*.json\0", "json", path, error);
}
bool BEditorDialog_SaveUIConfig(std::filesystem::path& path, std::string& error)
{
    return FileDialog(true, "BasilEditor UI Configs\0*.basilui.json\0JSON Files\0*.json\0", "basilui.json", path, error);
}
bool BEditorDialog_SelectFolder(std::filesystem::path& path, std::string& error)
{
    BROWSEINFOA browse{};
    browse.lpszTitle = "Select a BasilEngine Project location";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE item = SHBrowseForFolderA(&browse);
    if (!item) { error.clear(); return false; }
    char buffer[MAX_PATH]{};
    bool accepted = SHGetPathFromIDListA(item, buffer) != FALSE;
    CoTaskMemFree(item);
    if (!accepted) { error = "The selected folder could not be resolved."; return false; }
    path = buffer; error.clear(); return true;
}
bool BEditorPlatform_InstallCloseInterceptor(void* nativeWindow, std::string& error)
{
    closeWindow = static_cast<HWND>(nativeWindow);
    SetLastError(0);
    originalWindowProcedure = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(closeWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorWindowProcedure)));
    if (!originalWindowProcedure && GetLastError() != 0) { error = "Could not install native close protection."; closeWindow = nullptr; return false; }
    error.clear(); return true;
}
bool BEditorPlatform_TakeCloseRequest() { bool result = closeRequested; closeRequested = false; return result; }
void BEditorPlatform_RemoveCloseInterceptor()
{
    if (closeWindow && originalWindowProcedure) SetWindowLongPtrA(closeWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWindowProcedure));
    closeWindow = nullptr; originalWindowProcedure = nullptr; closeRequested = false;
}
#else
static bool Unsupported(std::string& error)
{
    error = "Native dialogs are not implemented on this platform yet.";
    return false;
}
bool BEditorDialog_OpenProject(std::filesystem::path&, std::string& error) { return Unsupported(error); }
bool BEditorDialog_OpenUIConfig(std::filesystem::path&, std::string& error) { return Unsupported(error); }
bool BEditorDialog_SaveUIConfig(std::filesystem::path&, std::string& error) { return Unsupported(error); }
bool BEditorDialog_SelectFolder(std::filesystem::path&, std::string& error) { return Unsupported(error); }
bool BEditorPlatform_InstallCloseInterceptor(void*, std::string& error) { error.clear(); return true; }
bool BEditorPlatform_TakeCloseRequest() { return false; }
void BEditorPlatform_RemoveCloseInterceptor() {}
#endif
