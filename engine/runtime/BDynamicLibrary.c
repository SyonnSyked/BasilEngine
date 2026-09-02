#include "BDynamicLibrary.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void SetError(char* output, size_t size, const char* message)
{
    if (output && size) snprintf(output, size, "%s", message ? message : "Dynamic library operation failed.");
}

bool BDynamicLibrary_Open(BDynamicLibrary* library, const char* path, char* error, size_t errorSize)
{
    if (!library || !path) { SetError(error, errorSize, "Dynamic library and path are required."); return false; }
    library->handle = 0;
#ifdef _WIN32
    library->handle = (void*)LoadLibraryA(path);
    if (!library->handle) { char message[128]; snprintf(message, sizeof(message), "Could not load game module (Windows error %lu).", (unsigned long)GetLastError()); SetError(error, errorSize, message); return false; }
#else
    library->handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!library->handle) { SetError(error, errorSize, dlerror()); return false; }
#endif
    SetError(error, errorSize, ""); return true;
}

void* BDynamicLibrary_Symbol(BDynamicLibrary* library, const char* name, char* error, size_t errorSize)
{
    if (!library || !library->handle || !name) { SetError(error, errorSize, "Loaded library and symbol name are required."); return 0; }
#ifdef _WIN32
    FARPROC procedure = GetProcAddress((HMODULE)library->handle, name);
    void* symbol = 0;
    memcpy(&symbol, &procedure, sizeof(symbol));
    if (!symbol) { SetError(error, errorSize, "Game module does not export BasilGame_Query."); return 0; }
#else
    dlerror(); void* symbol = dlsym(library->handle, name); const char* detail = dlerror();
    if (detail) { SetError(error, errorSize, detail); return 0; }
#endif
    SetError(error, errorSize, ""); return symbol;
}

void BDynamicLibrary_Close(BDynamicLibrary* library)
{
    if (!library || !library->handle) return;
#ifdef _WIN32
    FreeLibrary((HMODULE)library->handle);
#else
    dlclose(library->handle);
#endif
    library->handle = 0;
}
