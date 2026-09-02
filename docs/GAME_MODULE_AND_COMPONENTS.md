# Game Module and Project Components

This document records the Stage 4 alpha boundary between BasilEngine, native
Project code, and editor-readable component data.

## Native game module

Every generated C, C++, or mixed Project builds two artifacts: a small runtime
host executable and one platform-native game module. The module exports the
C-linkage `BasilGame_Query` function declared by `BGameModule.h`. Version 1
provides initialize, update, render-contribution, and shutdown callbacks.

The host owns engine state. Project code receives a versioned function table,
opaque entity handles, and bounded values; it does not receive pointers to
engine-owned Workspace objects. Version 1 exposes logging, the Project root,
entity enumeration and identity, position access, and read-only custom-component
JSON. C++ uses the same ABI and ownership rules as C.

At startup, the host loads `<ProjectIdentifier>.game.dll` on Windows (or the
corresponding `.so`/`.dylib` later), resolves the query function, and checks both
the API version and structure size before calling Project code. An incompatibility
is fatal and reports the expected and provided versions. Live replacement and
state migration remain deferred.

The editable Project CMake file links a candidate module first. Only a successful
link runs the post-build promotion into the canonical `.game` artifact. Thus a
compile or link failure leaves the last valid module intact. This is the same
seam that can support hot reload later, without implementing it during alpha.

## Project-defined components

Project component descriptions live in `.basil/components.json`, independently
of the native module. The root format is strict and versioned:

```json
{
  "schemaVersion": 1,
  "types": []
}
```

Each type has a stable `id`, `displayName`, positive `version`, and bounded
`fields`. Fields have stable `id`, `displayName`, `type`, and `default`; optional
`group`, numeric `min`/`max`, and enum `options` provide Inspector presentation
and constraints. Supported field types are `bool`, `int`, `float`, `string`,
`color`, `entity-ref`, `asset-ref`, and `enum`.

BasilEditor validates this file as data and never loads Project code to draw the
Inspector. Registered defaults can be attached to an entity through the
Inspector with normal Workspace undo/redo and save protection. Instance data is
stored as readable JSON in the Workspace component envelope. Custom components
are optional at this boundary: unknown optional data round-trips unchanged,
while unknown required data remains a load error.

## Stage 4 manual exit check (Windows)

1. Create one C, one C++, and one mixed Project and build/run each.
2. Confirm the generated `.game.dll` sits beside its executable.
3. Add a registered component in the Inspector, save, reopen, and confirm its
   data remains present without launching the game.
4. Introduce a compile error in `game.c`/`game.cpp`; confirm the build fails and
   the prior `.game.dll` remains unchanged and runnable after restoring source.
5. Build a module with a deliberately incompatible API version and confirm the
   runtime rejects it with both expected and provided versions.

Automated coverage performs the three language builds, incompatibility check,
failed-build preservation check, relocation check, metadata validation, and
Workspace custom-data round trip.
