# BasilEngine

BasilEngine is a C-first, cross-platform engine for building smooth, real-time
games whose primary visual language is ASCII and text art. C++ is reserved for
integrations that require it, such as Dear ImGui.

`WhereBirdsNest` is the reference game used to validate the engine. Its target
is a dark-fantasy action RPG that combines text-art presentation with smooth,
Diablo-like movement and combat.

The project is experimental. Its runtime and ASCII gameplay feasibility work is
now established; development is turning toward the project system and editor.
See the [project charter](docs/PROJECT_CHARTER.md), [roadmap](docs/ROADMAP.md),
[architecture notes](docs/ARCHITECTURE.md), and
[project-system specification](docs/PROJECT_SYSTEM.md) for the current direction.
The authoritative visual and interaction plan is recorded in the
[BasilEditor experience specification](docs/EDITOR_EXPERIENCE.md).

## Current capabilities

- Application and engine lifecycle
- raylib window and frame management
- Time and frame tracking
- Named keyboard input actions
- In-memory logging and an interactive developer console
- Layered ASCII rendering with per-cell foreground/background colors
- Plain-text ASCII asset loading and runtime glyph editing
- Smooth world-space movement, camera tracking, and collision
- A small `WhereBirdsNest` combat feasibility arena
- Versioned JSON project manifests and a headless empty-project generator
- A graphical BasilEditor project browser with New, Open, and Recent workflows
- Optional Git initialization during or after project creation
- Headless project, generated-build, input, canvas, and combat tests

## Reference demo

`WhereBirdsNest` currently provides a small movement and combat arena:

- Move with `WASD`.
- Attack the `D` target with `Space` when in range.
- Open the developer console with the backtick/grave key.
- Enter `help` in the console to list commands.

## Building on the current Windows development machine

Prerequisites:

- CMake 3.20 or newer
- Ninja
- UCRT64 GCC
- raylib
- BasilsTools
- Local Dear ImGui and rlImGui source trees only when editor dependencies are enabled

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

Set `BASIL_BUILD_EDITOR_DEPS=ON` to build `BasilEditor` using the vendored ImGui
and rlImGui source trees:

```powershell
cmake --preset ucrt64-debug -DBASIL_BUILD_EDITOR_DEPS=ON
cmake --build --preset ucrt64-debug
.\build\BasilEditor.exe
```

BasilEditor opens to its project browser. It can also open a project directly:

```powershell
.\build\BasilEditor.exe C:\Projects\MyGame\MyGame.basilproject
```

The demo executable is generated at `build/WhereBirdsNest.exe`.

## Creating an empty project

The current headless tool creates C-only, C++-only, or mixed projects. Mixed
C11/C++26 is the default:

```powershell
.\build\BasilProjectTool.exe create "My Game" MyGame C:\Projects
```

Launching `BasilProjectTool.exe` without command-line arguments opens an
interactive console wizard and waits for confirmation before closing.

Language rules can be selected without editing the generator:

```powershell
.\build\BasilProjectTool.exe create "My C Game" MyCGame C:\Projects `
    --language c --c-standard 17
```

Generated projects contain a versioned `.basilproject` manifest, editable CMake
configuration, an engine lifecycle entry point, empty asset/content directories,
and a suitable `.gitignore`. Configure one by supplying the current engine
source location and the same dependency hints used by BasilEngine:

```powershell
cmake -S C:\Projects\MyGame -B C:\Projects\MyGame\build `
    -DBASIL_ENGINE_ROOT=C:\path\to\BasilEngine `
    -DBASIL_RAYLIB_ROOT=C:\path\to\raylib `
    -DBASIL_TOOLS_ROOT=C:\path\to\BasilsTools
cmake --build C:\Projects\MyGame\build
```

## Repository layout

```text
engine/                 Reusable runtime systems
projects/wherebirdsnest Reference game and engine proving ground
editor/                 BasilEditor application
tests/                  Headless engine tests
thirdparty/             Local ImGui and rlImGui sources
docs/                   Product, roadmap, and architecture decisions
```

## Development rules

- The engine must not depend on Where Birds Nest.
- Engine abstractions are validated through a real reference-game use case.
- Runtime and editor will use the same Project and Workspace data.
- ASCII assets remain human-readable and useful outside the editor.
- Simulation coordinates are independent from glyph-cell coordinates.
- Platform-specific behavior stays behind narrow interfaces.
- New persistent data formats are versioned from their first revision.
- Milestone branches should build and pass tests before integration.
