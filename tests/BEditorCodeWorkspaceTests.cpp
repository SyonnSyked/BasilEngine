#include "BEditorCodeWorkspace.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static int Check(bool value, const char* message) { if (value) return 0; std::fprintf(stderr, "FAILED: %s\n", message); return 1; }
int main()
{
    int failures = 0; std::string error; fs::path root = fs::temp_directory_path() / ("basil-code-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "source"); std::ofstream(root / "source/game.c") << "int value = 1;\n"; fs::create_directories(root / "build/ignored"); std::ofstream(root / "build/ignored/file.c") << "ignored";
    BEditorCodeWorkspace code; failures += Check(code.OpenProject(root, error), "Project opens"); failures += Check(code.Files().size() == 1, "build output is excluded");
    failures += Check(code.OpenFile("source/game.c", error), "source opens"); failures += Check(code.SetText(0, "int value = 2;\n", error) && code.HasDirtyDocuments(), "edit becomes dirty");
    failures += Check(code.SaveAll(error) && !code.HasDirtyDocuments(), "Save All clears dirty state");
    failures += Check(code.CreateFile("source/new.hpp", error), "file creation works"); failures += Check(code.RenameFile("source/new.hpp", "source/new_name.hpp", error), "file rename works"); failures += Check(code.DeleteFile("source/new_name.hpp", error), "closed file deletion works");
    failures += Check(!code.OpenFile("../outside.c", error), "root escape is rejected");
    std::ofstream(root / "source/game.c", std::ios::binary | std::ios::trunc) << "external\n"; fs::last_write_time(root / "source/game.c", fs::file_time_type::clock::now() + std::chrono::seconds(2)); failures += Check(code.PollExternalChanges(error) && code.Documents()[0].text == "external\n", "clean external edit refreshes");
    code.SetText(0, "dirty\n", error); std::ofstream(root / "source/game.c", std::ios::binary | std::ios::trunc) << "conflict\n"; fs::last_write_time(root / "source/game.c", fs::file_time_type::clock::now() + std::chrono::seconds(4)); failures += Check(code.PollExternalChanges(error) && code.Documents()[0].externalConflict, "dirty external edit becomes conflict");
    fs::remove_all(root); if (!failures) std::printf("BEditorCodeWorkspaceTests passed.\n"); return failures ? 1 : 0;
}
