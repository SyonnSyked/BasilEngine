#ifndef BASIL_ENGINE_DYNAMIC_LIBRARY_H
#define BASIL_ENGINE_DYNAMIC_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BDynamicLibrary { void* handle; } BDynamicLibrary;
bool BDynamicLibrary_Open(BDynamicLibrary* library, const char* path, char* error, size_t errorSize);
void* BDynamicLibrary_Symbol(BDynamicLibrary* library, const char* name, char* error, size_t errorSize);
void BDynamicLibrary_Close(BDynamicLibrary* library);

#ifdef __cplusplus
}
#endif
#endif
