# BasilEditor Experience Specification

This is the authoritative record of the agreed BasilEditor product,
interaction, terminology, and visual direction. It exists to prevent feature
drift and mismatched workflows as the editor grows.

The implemented visual-foundation slice is closed out in
`EDITOR_VISUAL_FOUNDATION.md`. This specification remains authoritative for
both implemented behavior and future product decisions.

Status words are intentional:

- **Confirmed:** a product decision future work should preserve.
- **Implemented:** behavior present in the repository.
- **Planned:** an agreed outcome not implemented yet.
- **Deferred:** accepted direction deliberately outside the current stage.
- **Unresolved:** a decision that has not been made. Recommendations must not
  silently become requirements.

## Product experience

**Confirmed:** BasilEditor should feel like a focused cyberpunk development
workstation: a full NetRunner experience that remains mature, restrained, and
comfortable during long engineering sessions. Theme must support the work
rather than obscure it.

The editor provides first-party visual editing, project management, code
editing, build/run integration, diagnostics, and terminal access while
preserving normal files, CMake, Git, and external-editor workflows.

1. Visual tools are used where visual context provides real value.
2. Files remain readable and useful outside BasilEditor.
3. GUI, command-palette, and terminal actions share underlying operations.
4. No hidden build definition competes with user-editable CMake files.
5. Neovim and terminal development remain first-class even though the built-in
   editor is the default.
6. Empty Projects begin with a calm, slim interface.

## Canonical terminology

These terms are **confirmed** and must be used consistently in UI labels,
documentation, APIs, and future persistent formats.

### Project

A Project is the complete game or application. It owns its manifest, source,
assets, settings, build configuration, and one or more Workspaces. Its manifest
uses the `.basilproject` extension.

### Workspace

A Workspace is BasilEngine's name for the editable and loadable content unit
that other engines commonly call a scene. Its intended functional model is
substantially similar to Unity's scene model, but user-facing BasilEngine
terminology must use **Workspace**.

A Workspace may contain entities, environment content, component state, and
references to project assets and code. Expected vocabulary includes New
Workspace, Open Workspace, Save Workspace, Active Workspace, Workspace
Hierarchy, and loading an additional Workspace.

Multiple/additive Workspaces are intended, but their exact runtime and ownership
semantics remain **unresolved**. Workspace schema version 2 is **implemented**
as JSON using `.basilworkspace`. It supports a flat list of entities with stable
immutable IDs, editable names, and enabled state. Component serialization,
parenting, and runtime ownership remain deliberately undefined until their
reusable runtime models exist.

### Viewport

The Viewport visually represents the active Workspace and hosts the initial
in-editor development-play display.

### UI Config

A UI Config is a saved arrangement and visibility configuration for editor
panels. It replaces ambiguous phrases such as "editor workspace" because
Workspace already names Project content. UI Configs are not game content and do
not alter the active Workspace.

## Project lifecycle

The following behavior is **confirmed**:

- Launch without a Project opens the project browser.
- The browser exposes New Project, Open Project, and Recent Projects.
- Opening a `.basilproject` through the OS is intended to launch BasilEditor.
- Each BasilEditor window owns one Project.
- The configurable default creation directory is `Documents/BasilEngine`.
- New Projects contain essentials, not demo gameplay.
- Projects support C only, C++ only, or both.
- Mixed C11/C++26 is the default; standards remain configurable per Project.
- Git initialization is optional at creation and remains available afterward.
- Supported older manifests are backed up before migration.
- Newer unsupported manifests are refused clearly and never modified.

The browser, generation, opening, recents, Git option, and command-line manifest
opening are **implemented**. Native file dialogs and OS file association are
**planned**.

## Default editor composition

The intended default relationship is:

```text
+--------------------- Main toolbar ----------------------+
| Project | Workspace | Build | Run | Terminal | Settings |
+-------------+---------------------------+---------------+
| Hierarchy   |                           | Inspector     |
|             |     Primary Viewport      |               |
|             |                           |               |
+-------------+---------------------------+---------------+
| Assets | Console | Build Output | Problems | Terminal   |
+---------------------------------------------------------+
```

This is not a commitment to exact dimensions.

- The Viewport receives the largest default area as a practical and ceremonial
  indication that the created experience is central.
- Panels are dockable and resizable.
- The default empty-Project UI is slim.
- Additional panels and environment settings are easy to discover.
- The Terminal is easy to expand and collapse.

The core panel set is Workspace Hierarchy, Inspector, Asset Browser, Console,
Build Output, Problems, Terminal, and Code Editor. The Code Editor remains
planned; the other windows exist at scaffold level or better.

The full-viewport dockspace, main Project/Workspace/View command surface,
independently dockable Project Details and Workspace Viewport windows, stable
core-panel identities, and resettable default layout are **implemented**. The
default reserves the dominant center for the Viewport, side regions for Project
Details/Hierarchy and Inspector, and a lower tab region for operational panels.
Workspace Hierarchy, Inspector, Assets, Console, Build Output, Problems, and
Terminal scaffold windows are also **implemented** and independently dockable.
Hierarchy loads the startup Workspace and owns entity creation and selection;
Inspector edits the selected entity's name and enabled state. Assets safely
lists the real top-level asset directory, and Console reports current editor
status. Build Output and Problems are connected to the asynchronous build
service. Terminal still states clearly that shell hosting is not connected.

Save Workspace and Ctrl+S are implemented with full-model validation,
temporary-file writing, and a recovery `.bak`. The editor reports clean/dirty
state and guards the transition back to the Project Browser with Save, Discard,
and Cancel choices. Additional Workspace creation/opening, undo/redo, component
editing, parenting, and protection around every operating-system shutdown path
remain planned.

## UI Configs

The following behavior is **confirmed**:

- UI Configs use human-readable, versioned JSON.
- The current arrangement can be saved at any time and loaded without manual
  reconstruction.
- An internal window manages saved UI Configs.
- UI Configs can be imported and exported as ordinary files.
- Global UI Configs live in user configuration storage for cross-Project reuse.
- A UI Config becomes Project-owned only through an explicit user choice.
- Users may select a global default; Projects may explicitly override it.
- Reset to the maintained BasilEditor default remains available.

The versioned in-memory default UI Config model, validated layout ratios, stable
panel registry, and Reset Default UI Config action are **implemented**. Saving,
loading, importing, and exporting JSON UI Config files remain **planned**; raw
ImGui docking state is not being presented as the portable format.

The public JSON should describe stable panel identities, visibility, and
editor-owned layout concepts. Raw ImGui state must not be the only long-lived
format. The final extension is **unresolved**; `.basilui.json` is only a
candidate.

## Code editing

The following direction is **confirmed**:

- The built-in code editor is the default script editor.
- It retains the terminal/NetRunner aesthetic.
- The preferred editor is easily configurable.
- Switching internal/external editors uses a small, understandable command
  configuration rather than bespoke integrations.
- Neovim is an explicitly supported workflow.
- External edits must not break file watching, diagnostics, builds, running, or
  future hot reload.

The first practical internal editor is **planned** to support multiple tabs,
line numbers, C/C++/CMake/JSON/text syntax highlighting, find/replace, go to
line, unsaved-change handling, compiler diagnostics, file-and-line navigation,
indent/format commands, Open in External Editor, and Reveal in Terminal/File
Browser.

Language-server completion, semantic navigation, refactoring, and deeper IDE
features are **deferred** until the core workflow is useful. External-editor
definitions should use a simple executable/command, arguments, and documented
path/line/column placeholders; the exact schema is unresolved.

## Terminal

The Terminal is a **confirmed** first-class, expandable/collapsible panel.

The staged direction is:

1. Use a platform shell through a basic integrated panel or launch bridge.
2. Integrate build/task output without pretending it is a terminal emulator.
3. Eventually provide a custom terminal tab owned by BasilEditor.

Initial defaults are Windows PowerShell on Windows and the user's login shell
on macOS/Linux (commonly zsh and bash respectively). Shell choice remains
configurable. A correct custom terminal emulator is an accepted **deferred** end
goal because process control, input, resizing, and platform behavior are
substantial work.

## Commands and automation

Important operations should be accessible through visible buttons/menus, a
searchable command palette, and terminal-equivalent commands. These surfaces
must use a shared command/service layer rather than independent implementations.
This includes Project/Workspace creation and saving, import, build, test, run,
Git initialization, and opening tools. The palette and terminal command layer
are planned, not implemented.

## Development play

The first final-form development-play workflow is **confirmed** to run inside
the editor Viewport, with Play, Stop, and Pause where supported. Editor and
standalone runtime must use the same Project, Workspace, asset, and
serialization code. A separate game window may remain an option but is not the
intended final default. Hot reload follows only after the runtime module
boundary is stable.

An asynchronous standalone-process bridge is **implemented** as the reliable
precursor to that workflow. Build configures and compiles the Project through
its editable CMake files; Run builds and launches it; Pause/Resume and Stop
control the native process. Output and detected failures feed their dockable
panels without blocking the editor. The current generated runtime does not yet
consume Workspace entities, and the game window is not embedded in the
Viewport, so this bridge must not be described as in-editor development play.

## Notifications

The following behavior is **confirmed**:

- Routine success uses a compact notification/status indicator.
- Build/test activity does not automatically expand the Terminal.
- Failure produces a visible notification.
- Actionable failures make the Problems panel immediately visible.
- Build and terminal output remains available for inspection.
- Diagnostics navigate to the relevant source file and line.

Build-output capture, automatic Problems visibility on failure, and compact
status notifications are **implemented**. Diagnostic source navigation remains
planned until the code editor/file-opening service exists.

## Visual identity

The visual direction is **confirmed**:

- Mature, restrained cyberpunk/NetRunner aesthetic
- Very dark graphite or blue-black foundation
- Electric cyan primary accent
- Limited secondary bright colors, never a rainbow palette
- Monospace typography throughout
- ASCII/terminal-font art used for technical meaning and product identity
- Terminal-inspired panels, status displays, and empty states
- High readability and practical information hierarchy
- No flashy treatment that makes the tool tiring or difficult to parse

Amber warnings, controlled red errors, and restrained violet/magenta secondary
accents are current recommendations, not final brand constants. Interface scale
is a global editor preference with 100%, 115%, 135%, 150%, and 175% presets;
135% is the default.

The centralized ImGui theme, semantic palette, baseline spacing, rounded
geometry, and non-compounding scale application are **implemented** in the
BasilEditor theme module. Current and future panels should consume that module
rather than define unrelated local themes.

The implemented Project Browser and dockable editor shell consume this visual
system. The browser uses a restrained system rail, terminal-style status
language, structured Project actions, and clear full-width forms. The shell
uses independently dockable Project Details and Workspace Viewport windows;
the Viewport remains a deliberate placeholder that communicates the planned
next stage without presenting Workspace editing as implemented.

Global editor preferences are stored as versioned JSON in BasilEngine's user
configuration directory. Missing preferences use safe defaults. Malformed or
unsupported preferences are rejected with a visible explanation while the
editor continues with defaults. UI Config persistence remains a separate future
system because a UI Config describes panel layout, not global preferences.

### Typography

JetBrains Mono is the **confirmed** bundled typeface for consistent
cross-platform identity. Long-session readability and code legibility outrank
decorative novelty. Regular and Bold from the pinned v2.304 release are stored
with their SIL Open Font License in `assets/editor/fonts/JetBrainsMono`. The
editor build copies and loads them at startup. If either required file is
unavailable, BasilEditor remains usable with ImGui's fallback font and reports
the problem.

### Motion

Subtle cursor pulses, scans, status flickers, and similar effects are accepted
possible enhancements but **deferred** until essential workflows are mature.
Motion should communicate state, remain subtle, and eventually respect reduced
motion preferences.

## Application icon

The icon follows these confirmed constraints:

- Simple, sleek, mature, and unmistakably cyberpunk
- Compatible with the dark/electric-cyan palette
- Legible at small taskbar, window, and file-list sizes
- Not flashy, crowded, or overly detailed

A geometric basil leaf constructed from angular electric-cyan circuit segments,
with restrained violet nodes on a dark blue-black field, is **implemented** as
the BasilEditor application mark. The SVG master and reviewed PNG/ICO exports
live under `assets/editor/branding`. Windows builds embed the multi-resolution
ICO in the executable, and the running editor loads its PNG size set for window
identity.

Distinct `.basilproject` and Where Birds Nest identity assets remain planned.
Future macOS application-bundle packaging will require an ICNS export.

## Explicitly deferred

- Custom cross-platform terminal emulation
- Decorative cyberpunk animation
- Deep language-server/refactoring functionality
- Hot reload before the module boundary is stable
- Additive Workspace behavior before one Workspace is reliable
- A large plugin marketplace
- Wholesale replacement of specialized external development tools

## Remaining decisions

- Final `.basilproject` and Where Birds Nest icons
- Workspace component/parenting schema and additive semantics
- Portable UI Config JSON schema and extension
- External-editor configuration schema and placeholders
- Exact default docking measurements
- Native file-dialog strategy per platform
- Initial terminal hosting/launch approach

Gameplay questions remain in `OPEN_DESIGN_QUESTIONS.md` and must not distract
from editor-foundation work.
