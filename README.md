# BasilEngine

BasilEngine is a C-first, cross-platform engine for building smooth, real-time
games whose primary visual language is ASCII and text art. C++ is reserved for
integrations that require it, such as Dear ImGui.

`WhereBirdsNest` is the reference game used to validate the engine. Its target
is a dark-fantasy action RPG that combines text-art presentation with smooth,
Diablo-like movement and combat.

The project is experimental. The Project/editor workflow and pre-audio runtime
services are established; bounded audio is the next Stage 6 service.
See the [project charter](docs/PROJECT_CHARTER.md), [roadmap](docs/ROADMAP.md),
[architecture notes](docs/ARCHITECTURE.md), and
[project-system specification](docs/PROJECT_SYSTEM.md) for the current direction.
The authoritative visual and interaction plan is recorded in the
[BasilEditor experience specification](docs/EDITOR_EXPERIENCE.md). The completed
editor visual-foundation pass and its verification evidence are summarized in
the [visual-foundation closeout](docs/EDITOR_VISUAL_FOUNDATION.md). The current
capabilities, roadmap alignment, risks, scope guardrails, and next bounded slice
are tracked in the [scope and course audit](docs/SCOPE_STATUS.md). The
[Runtime Workspace Bridge](docs/RUNTIME_WORKSPACE_BRIDGE.md) is a historical
implementation closeout, followed by the implemented
[game-module and Project-component boundary](docs/GAME_MODULE_AND_COMPONENTS.md).
The integrated edit/build/diagnose/run loop is specified and checked in the
[programming workflow closeout](docs/INTEGRATED_PROGRAMMING_WORKFLOW.md).
The requirements and measurable completion boundary for the current final
stretch are defined in the [Alpha Product Contract](docs/ALPHA_PRODUCT_CONTRACT.md).

## Current capabilities

- Application and engine lifecycle
- raylib window and frame management
- Time and frame tracking
- Named keyboard/mouse input actions with runtime rebinding
- In-memory logging and an interactive developer console
- Layered ASCII rendering with per-cell foreground/background colors
- Plain-text ASCII asset loading and runtime glyph editing
- Smooth world-space movement, camera tracking, and collision
- A canonical Where Birds Nest Project integrating movement, Collider2D room
  bounds, trigger interaction, HUD/dialogue, safe Workspace replacement, and
  generation-safe entity-handle reacquisition
- Versioned JSON Project and empty-Workspace formats with a headless generator
- A graphical BasilEditor project browser with New, Open, and Recent workflows
- A centralized cyberpunk editor theme with bundled JetBrains Mono typography
- NetRunner-styled Project Browser, Project Details, and Workspace Viewport
- A genuine dockable editor shell with a resettable slim default UI Config
- Dockable Hierarchy, Inspector, Assets, Console, Build Output, Problems, and
  Terminal scaffolds with honest service states
- Editable startup Workspaces with stable entity IDs, selection, Inspector
  changes, dirty-state protection, and backup-assisted saves
- Lifecycle-managed Workspace documents with transactional load/clone and
  structured diagnostics
- Workspace schema v4 with stable AssetRefs, versioned Transform2D/ASCII
  Renderable/Collider2D components, schema v3 migration, and preservation of
  unknown optional component data
- A bounded, project-root-contained Text Sprite decoder with transparent-space
  grids, structured diagnostics, and last-known-good cache replacement
- Deterministic host-neutral ASCII draw-list interpretation with shared anchor,
  layer, visibility, transparency, color, and source-entity semantics
- Generated runtimes that discover or accept their Project manifest, load the
  startup Workspace, render its draw list, and show stable empty/error states
- Editor authoring for visible glyphs, Project Text Sprites, and empty entities,
  with validated Transform2D and ASCII Renderable Inspector controls
- Run preflight with complete Text Sprite validation, safe Workspace save,
  explicit manifest launch arguments, and structured editor Problems
- A shared-data authoring Viewport with glyph/Text Sprite previews, grid,
  pan/zoom, selection outlines, and editor-only empty-entity markers
- Asynchronous CMake Build and Run controls with streamed output, extracted
  problems, and native Pause, Resume, and Stop process control
- Geometric circuit-leaf application branding and embedded Windows icon
- Persistent global interface-scale presets from 100% through 175%
- Optional Git initialization during or after project creation
- Project-root Git detection that avoids redundant initialization controls
- Workspace undo/redo, safe duplication, recovery snapshots, and native-window
  unsaved-change protection
- Portable JSON UI Configs with global/Project precedence, import, and export
- Native Windows Project, folder, and UI Config dialogs
- Case-insensitive Hierarchy and asset filtering
- Stable Project asset registry for Text Sprites, JSON data, fonts, and audio,
  with external change/move detection and reference repair
- Dockable bounded Text Sprite editor with transactional preview, safe save,
  transparent-space visualization, and external-edit conflict handling
- Versioned C-compatible native game modules for C, C++, and mixed Projects,
  with compatibility checks and last-valid artifact preservation
- Strict Project component metadata and human-readable custom Workspace data,
  inspected without executing Project code
- Public collision queries and safe transactional replacement of the one active
  Workspace, including generation-safe entity handles
- Screen-space ASCII labels, panels, anchored HUDs, and keyboard/mouse choices
  through the public C/C++ game API
- Headless Project, generated-build, input, draw-list, runtime-service, and WBN tests

## Reference demo

`projects/wherebirdsnest` is the canonical reference Project:

- Move with `WASD`.
- Approach Seamus and press Enter to open dialogue.
- Use `W`/`S` or the mouse to select a response; movement is suppressed while
  dialogue is open.
- Choose Yes to transition safely to Testing1; the game observes the generation
  change, reacquires Wayfinder, and continues moving.

The older `WBNCombat` code remains as a separately tested feasibility spike; it
is not the canonical demo or build path.

## Building on the current Windows development machine

Prerequisites:

- CMake 3.25 or newer
- Ninja
- A C11/C++17 toolchain (UCRT64 GCC is the currently verified Windows setup)
- raylib
- Local Dear ImGui docking-branch and rlImGui source trees only when editor
  dependencies are enabled

Configure dependency locations once in a machine-local preset. Copy
`CMakeUserPresets.json.example` to `CMakeUserPresets.json`, update the toolchain
and raylib paths,
and keep that local file uncommitted. BasilEngine first looks for installed
CMake packages, then pkg-config for raylib, and finally searches the supplied
roots for headers and libraries.

```powershell
Copy-Item CMakeUserPresets.json.example CMakeUserPresets.json
# Edit CMAKE_TOOLCHAIN_FILE and BASIL_RAYLIB_ROOT once, then:
cmake --preset local-headless-debug
cmake --build --preset local-headless-debug
ctest --preset local-headless-debug
```

On macOS or Linux, use any preferred generator and provide the same cache hints
when the dependencies are not installed system-wide:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DBASIL_RAYLIB_ROOT=/path/to/raylib
cmake --build build
ctest --test-dir build --output-on-failure
```

The editor preset enables the vendored ImGui and rlImGui source trees.
BasilEditor requires the official ImGui `docking` branch; configuration stops
with an actionable error if a master-branch checkout is supplied:

```powershell
cmake --preset local-dev
cmake --build --preset local-dev
ctest --preset local-dev
.\build\dev\BasilEditor.exe
```

BasilEditor opens to its project browser. It can also open a project directly:

```powershell
.\build\dev\BasilEditor.exe C:\Projects\MyGame\MyGame.basilproject
```

Where Birds Nest has one build definition in its Project directory and can be
built through BasilEditor or its editable `projects/wherebirdsnest/CMakeLists.txt`.

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
configuration, developer source using only `BGame.h`, empty asset/content
directories, and a suitable `.gitignore`. Basil supplies the executable entry
point and native-module registration glue. Configure one by supplying the current engine
source location and the same dependency hints used by BasilEngine:

```powershell
cmake -S C:\Projects\MyGame -B C:\Projects\MyGame\build `
    -DBASIL_ENGINE_ROOT=C:\path\to\BasilEngine `
    -DBASIL_RAYLIB_ROOT=C:\path\to\raylib
cmake --build C:\Projects\MyGame\build
```

Generated C++ and mixed Projects also enable `BasilGLM`, backed by GLM 1.0.3
at pinned commit `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed`. C-only Projects do not resolve
or link GLM. C++ game code can use it directly:

```cpp
#include <glm/glm.hpp>

glm::vec2 position{0.0f, 0.0f};
glm::vec2 velocity{2.0f, -1.0f};
position += velocity * deltaTime;
```

An installed GLM package is preferred. `BASIL_GLM_ROOT` can identify a local
install or source tree, while generated C++/mixed Projects enable the pinned
FetchContent fallback by default. `BGame.h` and the native game ABI remain
C-compatible and expose no GLM types.

## Repository layout

```text
engine/                 Reusable runtime systems
projects/wherebirdsnest Reference game and engine proving ground
editor/                 BasilEditor application
assets/editor/          Bundled editor fonts and branding assets
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
