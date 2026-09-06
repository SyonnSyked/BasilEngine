#include "BGameModule.h"

#include <string.h>

BGAME_MODULE_EXPORT bool BasilGame_Query(uint32_t hostVersion, BGameModule *module)
{
    if (module == NULL) {
        return false;
    }

    memset(module, 0, sizeof(*module));
    module->version = BGAME_API_VERSION;
    module->structSize = sizeof(*module);
    if (hostVersion != BGAME_API_VERSION) {
        return false;
    }
    module->name = "Basil Game";
    module->onInitialize = BasilGame_Initialize;
    module->onUpdate = BasilGame_Update;
    module->onRender = BasilGame_Render;
    module->onShutdown = BasilGame_Shutdown;
    return true;
}
