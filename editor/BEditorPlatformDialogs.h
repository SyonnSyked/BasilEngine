#ifndef BASIL_EDITOR_PLATFORM_DIALOGS_H
#define BASIL_EDITOR_PLATFORM_DIALOGS_H

#include <filesystem>
#include <string>

bool BEditorDialog_OpenProject(std::filesystem::path& path, std::string& error);
bool BEditorDialog_OpenUIConfig(std::filesystem::path& path, std::string& error);
bool BEditorDialog_SaveUIConfig(std::filesystem::path& path, std::string& error);
bool BEditorDialog_SaveTextSprite(std::filesystem::path& path, std::string& error);
bool BEditorDialog_SelectFolder(std::filesystem::path& path, std::string& error);
bool BEditorPlatform_InstallCloseInterceptor(void* nativeWindow, std::string& error);
bool BEditorPlatform_TakeCloseRequest();
void BEditorPlatform_RemoveCloseInterceptor();
bool BEditorPlatform_OpenExternalEditor(const std::filesystem::path& file, std::string& error);
bool BEditorPlatform_RevealFile(const std::filesystem::path& file, std::string& error);
bool BEditorPlatform_OpenTerminal(const std::filesystem::path& directory, std::string& error);

#endif
