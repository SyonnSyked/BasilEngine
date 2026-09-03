#include "BTextSprite.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define MAKE_DIRECTORY(path) _mkdir(path)
#define REMOVE_DIRECTORY(path) _rmdir(path)
#define GET_PROCESS_ID() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIRECTORY(path) mkdir(path, 0755)
#define REMOVE_DIRECTORY(path) rmdir(path)
#define GET_PROCESS_ID() getpid()
#endif

static int Check(bool condition, const char* message)
{
    if (condition)
        return 0;

    fprintf(stderr, "FAILED: %s\n", message);
    return 1;
}

static bool WriteBytes(const char* path, const void* bytes, size_t length)
{
    FILE* file = fopen(path, "wb");

    if (file == NULL)
        return false;

    bool succeeded = fwrite(bytes, 1, length, file) == length;
    return fclose(file) == 0 && succeeded;
}

static const BDiagnostic* FirstError(const BDiagnosticList* diagnostics)
{
    return BDiagnosticList_FirstError(diagnostics);
}

int main(void)
{
    int failures = 0;
    char root[256];
    char assets[320];
    char spritePath[384];
    snprintf(root, sizeof(root), "TextSprite Test %ld %d", (long)time(NULL), (int)GET_PROCESS_ID());
    snprintf(assets, sizeof(assets), "%s/assets", root);
    snprintf(spritePath, sizeof(spritePath), "%s/hero.txt", assets);
    failures += Check(MAKE_DIRECTORY(root) == 0, "temporary Project root is created");
    failures += Check(MAKE_DIRECTORY(assets) == 0, "temporary asset directory is created");

    const char valid[] = " /\\  \r\n<@@>\r\n /\\\r\n";
    failures += Check(WriteBytes(spritePath, valid, sizeof(valid) - 1), "valid CRLF Text Sprite is written");

    BDiagnosticList diagnostics;
    BTextSprite sprite;
    BTextSprite_Init(&sprite);
    failures += Check(BTextSprite_Load(root, "assets/hero.txt", &sprite, &diagnostics), "valid Text Sprite loads");
    failures += Check(sprite.width == 4 && sprite.height == 3, "uneven rows decode to deterministic bounds");
    failures += Check(
        BTextSprite_Cell(&sprite, 0, 0) == ' ' && BTextSprite_Cell(&sprite, 3, 0) == ' ' &&
            BTextSprite_Cell(&sprite, 3, 1) == '>' && BTextSprite_Cell(&sprite, 3, 2) == ' ',
        "leading, trailing, and padded spaces remain transparent cells"
    );

    const char invalidTab[] = "ok\nX\tY\n";
    failures += Check(WriteBytes(spritePath, invalidTab, sizeof(invalidTab) - 1), "invalid tab fixture is written");
    failures += Check(!BTextSprite_Load(root, "assets/hero.txt", &sprite, &diagnostics), "tab is rejected");
    const BDiagnostic* diagnostic = FirstError(&diagnostics);
    failures += Check(
        diagnostic != NULL && diagnostic->code == BDIAGNOSTIC_INVALID_DATA &&
            diagnostic->line == 2 && diagnostic->column == 2 &&
            strcmp(diagnostic->path, "assets/hero.txt") == 0,
        "invalid byte reports path, line, and column"
    );
    failures += Check(sprite.width == 4 && sprite.height == 3, "failed reload preserves the destination sprite");

    const unsigned char bom[] = { 0xef, 0xbb, 0xbf, '@', '\n' };
    failures += Check(WriteBytes(spritePath, bom, sizeof(bom)), "BOM fixture is written");
    failures += Check(!BTextSprite_Load(root, "assets/hero.txt", &sprite, &diagnostics), "UTF-8 BOM is rejected");

    const char loneCarriageReturn[] = "A\rB";
    failures += Check(WriteBytes(spritePath, loneCarriageReturn, sizeof(loneCarriageReturn) - 1), "lone CR fixture is written");
    failures += Check(!BTextSprite_Load(root, "assets/hero.txt", &sprite, &diagnostics), "lone carriage return is rejected");

    const char spacesOnly[] = "   \n  \n";
    failures += Check(WriteBytes(spritePath, spacesOnly, sizeof(spacesOnly) - 1), "spaces-only fixture is written");
    failures += Check(!BTextSprite_Load(root, "assets/hero.txt", &sprite, &diagnostics), "spaces-only Text Sprite is rejected");

    char tooWide[BTEXT_SPRITE_WIDTH_MAX + 2];
    memset(tooWide, 'A', sizeof(tooWide));
    failures += Check(WriteBytes(spritePath, tooWide, sizeof(tooWide)), "over-wide fixture is written");
    failures += Check(!BTextSprite_Load(root, "assets/hero.txt", &sprite, &diagnostics), "over-wide Text Sprite is rejected");

    size_t tooTallLength = (BTEXT_SPRITE_HEIGHT_MAX + 1) * 2;
    char* tooTall = (char*)malloc(tooTallLength);
    failures += Check(tooTall != NULL, "over-tall fixture allocation succeeds");

    if (tooTall != NULL)
    {
        for (size_t i = 0; i < tooTallLength; i += 2)
        {
            tooTall[i] = 'A';
            tooTall[i + 1] = '\n';
        }

        failures += Check(WriteBytes(spritePath, tooTall, tooTallLength), "over-tall fixture is written");
        failures += Check(!BTextSprite_Load(root, "assets/hero.txt", &sprite, &diagnostics), "over-tall Text Sprite is rejected");
        free(tooTall);
    }

    failures += Check(
        !BTextSprite_Load(root, "../outside.txt", &sprite, &diagnostics) &&
            FirstError(&diagnostics) != NULL && FirstError(&diagnostics)->code == BDIAGNOSTIC_INVALID_ARGUMENT,
        "parent traversal is rejected"
    );
    failures += Check(!BTextSprite_Load(root, "/absolute.txt", &sprite, &diagnostics), "absolute path is rejected");
    failures += Check(!BTextSprite_Load(root, "assets\\hero.txt", &sprite, &diagnostics), "backslash path is rejected");

    const char cached[] = "ABC\n D\n";
    failures += Check(WriteBytes(spritePath, cached, sizeof(cached) - 1), "cache fixture is written");
    BTextSpriteCache cache;
    BTextSpriteCache_Init(&cache);
    const BTextSprite* cachedSprite = NULL;
    failures += Check(
        BTextSpriteCache_Load(&cache, root, "assets/hero.txt", &cachedSprite, &diagnostics),
        "valid Text Sprite enters cache"
    );
    failures += Check(cachedSprite != NULL && cachedSprite->width == 3 && cachedSprite->height == 2, "cached shape matches");
    failures += Check(WriteBytes(spritePath, invalidTab, sizeof(invalidTab) - 1), "invalid cache refresh is written");
    failures += Check(
        !BTextSpriteCache_Load(&cache, root, "assets/hero.txt", &cachedSprite, &diagnostics),
        "invalid cache refresh fails"
    );
    const BTextSprite* retained = BTextSpriteCache_Find(&cache, "assets/hero.txt");
    failures += Check(
        retained != NULL && retained->width == 3 && retained->height == 2 &&
            BTextSprite_Cell(retained, 2, 0) == 'C',
        "failed cache refresh retains last known-good sprite"
    );

    BTextSpriteCache_Destroy(&cache);
    BTextSpriteCache_Destroy(&cache);
    BTextSprite_Destroy(&sprite);
    BTextSprite_Destroy(&sprite);
    remove(spritePath);
    failures += Check(REMOVE_DIRECTORY(assets) == 0, "temporary asset directory is removed");
    failures += Check(REMOVE_DIRECTORY(root) == 0, "temporary Project root is removed");

    if (failures == 0)
        printf("BTextSpriteTests passed.\n");

    return failures == 0 ? 0 : 1;
}
