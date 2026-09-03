#include "BEditorAssetService.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static int Check(bool value, const char* message) { if (value) return 0; std::fprintf(stderr, "FAILED: %s\n", message); return 1; }
static void Write(const fs::path& path, const char* value) { fs::create_directories(path.parent_path()); std::ofstream(path, std::ios::binary) << value; }

int main()
{
    int failures = 0; std::string error; std::vector<BEditorAssetMove> moves;
    fs::path root = fs::temp_directory_path() / ("basil-assets-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Write(root / "assets/hero.txt", "<@>\n"); Write(root / "assets/data/items.json", "{}\n");
    Write(root / "assets/font.ttf", "fake-font"); Write(root / "assets/sound.ogg", "fake-audio"); Write(root / "assets/ignored.png", "ignored");
    BEditorAssetService assets;
    failures += Check(assets.Open(root, error), "asset service opens Project");
    failures += Check(assets.Records().size() == 4, "alpha asset kinds are catalogued");
    auto hero = std::find_if(assets.Records().begin(), assets.Records().end(), [](const auto& item) { return item.relativePath == "assets/hero.txt"; });
    failures += Check(hero != assets.Records().end(), "Text Sprite is present");
    std::string heroId = hero == assets.Records().end() ? "" : hero->id;
    fs::rename(root / "assets/hero.txt", root / "assets/player.txt");
    failures += Check(assets.Refresh(moves, error), "external move refresh succeeds");
    hero = std::find_if(assets.Records().begin(), assets.Records().end(), [](const auto& item) { return item.relativePath == "assets/player.txt"; });
    failures += Check(hero != assets.Records().end() && hero->id == heroId, "external move preserves stable asset ID");
    failures += Check(moves.size() == 1 && moves[0].oldPath == "assets/hero.txt" && moves[0].newPath == "assets/player.txt", "external move is reported for reference repair");
    failures += Check(fs::is_regular_file(root / ".basil/assets.json"), "asset registry is Project-owned JSON");
    failures += Check(assets.Rename("assets/player.txt", "pilot.txt", moves, error), "editor rename succeeds");
    hero = std::find_if(assets.Records().begin(), assets.Records().end(), [](const auto& item) { return item.relativePath == "assets/pilot.txt"; });
    failures += Check(hero != assets.Records().end() && hero->id == heroId, "editor rename preserves stable ID");
    BEditorAssetService reopened;
    failures += Check(reopened.Open(root, error), "asset registry reopens");
    hero = std::find_if(reopened.Records().begin(), reopened.Records().end(), [](const auto& item) { return item.relativePath == "assets/pilot.txt"; });
    failures += Check(hero != reopened.Records().end() && hero->id == heroId, "stable ID survives process restart");
    fs::remove_all(root);
    if (!failures) std::printf("BEditorAssetServiceTests passed.\n");
    return failures ? 1 : 0;
}
