#include "BEditorTextSpriteDocument.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static int Check(bool value, const char* message) { if (value) return 0; std::fprintf(stderr, "FAILED: %s\n", message); return 1; }

int main()
{
    int failures = 0; std::string error;
    fs::path root = fs::temp_directory_path() / ("basil-text-editor-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "assets"); std::ofstream(root / "assets/hero.txt", std::ios::binary) << "<@>\n";
    BEditorTextSpriteDocument document;
    failures += Check(document.Open(root, "assets/hero.txt", error), "Text Sprite document opens");
    failures += Check(document.Preview().width == 3 && document.Preview().height == 1, "Text Sprite preview decodes");
    std::strcpy(document.Buffer(), "/\\\n<@>\n"); document.MarkEdited();
    failures += Check(document.RefreshPreview(error) && document.Preview().height == 2, "edited text previews immediately");
    failures += Check(document.Save(error), "valid edit saves safely");
    failures += Check(fs::is_regular_file(root / "assets/hero.txt.bak"), "Text Sprite save retains backup");
    std::strcpy(document.Buffer(), "bad\tasset\n"); document.MarkEdited();
    failures += Check(!document.Save(error), "invalid edit cannot replace asset");
    failures += Check(document.IsDirty(), "failed save remains dirty");
    BEditorTextSpriteDocument disk;
    failures += Check(disk.Open(root, "assets/hero.txt", error) && disk.Preview().height == 2, "failed save preserves last valid disk asset");
    std::ofstream(root / "assets/hero.txt", std::ios::binary | std::ios::trunc) << "X\n";
    fs::last_write_time(root / "assets/hero.txt", fs::last_write_time(root / "assets/hero.txt") + std::chrono::seconds(1));
    failures += Check(disk.HasExternalChange(), "external Text Sprite edit is detected");
    failures += Check(disk.Reload(error) && disk.Preview().width == 1, "external edit reloads");
    BEditorTextSpriteDocument created;
    failures += Check(created.Create(root, "assets/new.txt", error), "new Text Sprite is created with valid starter content");
    failures += Check(!created.Open(root, "../escape.txt", error), "Text Sprite editor rejects escaped paths");
    fs::remove_all(root);
    if (!failures) std::printf("BEditorTextSpriteDocumentTests passed.\n");
    return failures ? 1 : 0;
}
