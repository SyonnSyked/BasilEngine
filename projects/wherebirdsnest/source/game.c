#include "BGameModule.h"

#include <string.h>

static bool Game_Initialize(const BGameHostAPI* host, void** gameState)
{
    (void)gameState;
    host->log(host->context, "Where Birds Nest game module initialized.");
    return true;
}

static void Game_Update(void* gameState, float deltaTime) { (void)gameState; (void)deltaTime; }
static void Game_Render(void* gameState) { (void)gameState; }
static void Game_Shutdown(void* gameState) { (void)gameState; }

BGAME_MODULE_EXPORT bool BasilGame_Query(uint32_t hostVersion, BGameModule* module)
{
    if (hostVersion != BGAME_API_VERSION || module == 0) return false;
    memset(module, 0, sizeof(*module));
    module->version = BGAME_API_VERSION;
    module->structSize = sizeof(*module);
    module->name = "WhereBirdsNest";
    module->onInitialize = Game_Initialize;
    module->onUpdate = Game_Update;
    module->onRender = Game_Render;
    module->onShutdown = Game_Shutdown;
    return true;
}
