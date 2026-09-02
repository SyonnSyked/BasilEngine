# BasilEngine Architecture

## System boundary

BasilEngine provides reusable mechanisms: application lifecycle, platform
services, input, time, rendering, audio, assets, Workspaces, serialization, project
loading, and diagnostics.

Game projects provide policy and content: characters, enemies, abilities,
items, quests, dialogue, encounters, and game-specific systems.

The engine must never include or branch on Where Birds Nest concepts.

## Intended product components

```text
Basil Launcher
  Creates, locates, and opens projects

Basil Editor
  Edits project assets and Workspaces using public engine APIs

Basil Runtime
  Runs reusable engine systems and a project game module

Where Birds Nest
  Supplies game code, data, Workspaces, and text-art assets
```

## Language boundary

Public engine-facing interfaces should remain C-compatible. The runtime is C11.
C++ editor code may wrap those interfaces but must not require the C runtime to
adopt C++ ownership, exceptions, or ABI conventions.

## Coordinate spaces

The engine will distinguish at least three spaces:

- **World space:** floating-point simulation positions and collision
- **Glyph space:** cells within a text-art asset or logical canvas
- **Screen space:** final pixel coordinates after camera and viewport transforms

Continuous simulation must not be constrained to integer glyph cells. Snapping
is an optional behavior, not an architectural assumption.

## Data and editor model

The editor and standalone runtime must use the same Project, Workspace, asset, and
serialization implementations. Editor panels should mutate data through explicit
APIs rather than reaching directly into arbitrary runtime memory. This preserves
validation and leaves a path to undo/redo.

Workspace is the canonical term for the loadable content unit that other
engines commonly call a scene. UI Config is the canonical term for a saved
arrangement of editor panels. These concepts must remain distinct.

Editor buttons, command-palette actions, and terminal commands should call a
shared command/service layer. The built-in editor is the default for scripts,
while normal files and configurable external-editor commands preserve Neovim-
and terminal-oriented workflows.

Persistent formats are versioned from their first revision. Project paths are
relative to the project root unless an explicitly external resource is needed.

## Alpha module boundary

Game code will compile as a DLL, shared object, or dynamic library and
communicate through a versioned C interface. The first Windows implementation
is an alpha requirement; the interface is versioned and testable but is not a
promise of post-alpha ABI stability.

Long-lived engine resources should be represented across the module boundary by
stable handles rather than internal pointers. Failed builds preserve the last
valid artifact. The boundary must leave room for later hot reload, but live
replacement and state migration are outside the first alpha. When reload is
implemented, state must either be engine-owned or serializable across reloads.

Project-defined Inspector fields use a separate, restricted metadata artifact.
BasilEditor must not load arbitrary Project code merely to inspect a Workspace.
The exact alpha boundary and supported field types are defined in
`ALPHA_PRODUCT_CONTRACT.md`.

## Current runtime flow

```text
Project main
  -> BApplication_Init
     -> BEngine_Init
        -> window, console, input
     -> game onStart
  -> BApplication_Run
     -> time and console update
     -> game onUpdate
     -> begin frame
     -> game onRender
     -> console overlay
     -> end frame
  -> game onShutdown
  -> BEngine_Shutdown
```

This flow is the current implementation, not a permanent promise. Global
subsystem state should move behind explicit ownership as systems mature.
