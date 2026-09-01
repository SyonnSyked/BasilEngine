# BasilEngine

BasilEngine is a C-first, cross-platform engine for building smooth, real-time
games whose primary visual language is ASCII and text art. C++ is reserved for
integrations that require it, such as Dear ImGui.

`WhereBirdsNest` is the reference game used to validate the engine. Its target
is a dark-fantasy action RPG that combines text-art presentation with smooth,
Diablo-like movement and combat.

The project is experimental and currently in its foundation stage. See the
[project charter](docs/PROJECT_CHARTER.md), [roadmap](docs/ROADMAP.md), and
[architecture notes](docs/ARCHITECTURE.md) before making broad changes.

## Current capabilities

- Application and engine lifecycle
- raylib window and frame management
- Time and frame tracking
- Named keyboard input actions
- In-memory logging and an interactive developer console
- Layered ASCII canvas rendering backed by BasilsTools
- A small `WhereBirdsNest` cabin demo

## Building on the current Windows development machine

Prerequisites:

- CMake 3.20 or newer
- Ninja
- UCRT64 GCC
- raylib
- BasilsTools
- Local Dear ImGui and rlImGui source trees

Configure dependency locations with `BASIL_RAYLIB_ROOT` and
`BASIL_TOOLS_ROOT`. BasilEngine first looks for installed CMake packages, then
pkg-config for raylib, and finally searches the supplied roots for headers and
libraries.

```powershell
cmake --preset ucrt64-debug `
    -DBASIL_RAYLIB_ROOT=C:/path/to/raylib `
    -DBASIL_TOOLS_ROOT=C:/path/to/BasilsTools
cmake --build --preset ucrt64-debug
ctest --test-dir build --output-on-failure
```

On macOS or Linux, use any preferred generator and provide the same cache hints
when the dependencies are not installed system-wide:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DBASIL_RAYLIB_ROOT=/path/to/raylib \
  -DBASIL_TOOLS_ROOT=/path/to/BasilsTools
cmake --build build
ctest --test-dir build --output-on-failure
```

Set `BASIL_BUILD_EDITOR_DEPS=ON` only when the local ImGui and rlImGui source
trees are available. They remain off by default until an editor target uses
them.

The demo executable is generated at `build/WhereBirdsNest.exe`.

## Repository layout

```text
engine/                 Reusable runtime systems
projects/wherebirdsnest Reference game and engine proving ground
editor/                 Planned editor application
tests/                  Headless engine tests
thirdparty/             Local ImGui and rlImGui sources
docs/                   Product, roadmap, and architecture decisions
```

## Development rules

- The engine must not depend on Where Birds Nest.
- Engine abstractions are validated through a real reference-game use case.
- Runtime and editor will use the same project and scene data.
- ASCII assets remain human-readable and useful outside the editor.
- Simulation coordinates are independent from glyph-cell coordinates.
- Platform-specific behavior stays behind narrow interfaces.
- New persistent data formats are versioned from their first revision.
- Milestone branches should build and pass tests before integration.
