#include "BGameModule.h"

#include <stdio.h>

bool BasilGame_Initialize(const BGameHostAPI *host, void **state)
{ (void)host; (void)state; return true; }
void BasilGame_Update(void *state, float deltaTime) { (void)state; (void)deltaTime; }
void BasilGame_Render(void *state) { (void)state; }
void BasilGame_Shutdown(void *state) { (void)state; }

int main(void)
{
    BGameModule module = {0};
    if (BasilGame_Query(BGAME_API_VERSION + 1, &module) ||
        module.version != BGAME_API_VERSION || module.structSize != sizeof(module)) {
        fprintf(stderr, "Incompatible query did not report compiled ABI metadata.\n");
        return 1;
    }
    if (!BasilGame_Query(BGAME_API_VERSION, &module) ||
        module.version != BGAME_API_VERSION || module.onInitialize != BasilGame_Initialize) {
        fprintf(stderr, "Current query did not register the game module.\n");
        return 1;
    }
    printf("BGameModuleGlueTests passed.\n");
    return 0;
}
