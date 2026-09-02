# BasilEngine Project System

This document records the agreed direction for creating and opening independent
BasilEngine projects. The versioned manifest, validation, JSON persistence,
versioned empty Workspace format, headless generator, and independent
generated-project build are implemented.
The BasilEditor project browser and initial dockable shell are implemented. The
browser creates and opens projects through shared headless project APIs, tracks
recent projects, accepts a manifest path on the command line, and exposes an
opened Project through independently dockable Project Details and Workspace
Viewport windows. The remaining core panel windows are present as dockable
scaffolds, including a read-only top-level asset listing. Startup Workspace
entities can now be edited and saved through the Hierarchy and Inspector.
The broader alpha asset/code/export workflow remains future work. Editor
component controls, runtime Workspace consumption, native Windows Project/folder
dialogs, document history/recovery, and portable UI Config persistence are
implemented.

## User workflow

Launching BasilEditor will open a project browser with New Project, Open
Project, and recent-project options. Projects may live anywhere on disk, with a
user-configurable default creation directory.

Creating a project will generate the essential foundation required to edit,
build, and run it. Empty projects will not include demo gameplay or decorative
sample content, and users will not need to manually connect ordinary
BasilEngine functionality.

The initial end-to-end acceptance target is:

1. Launch BasilEditor.
2. Create a named project in a selected directory.
3. Open the generated project immediately.
4. Display its project information and empty starter Workspace.
5. Edit and save the project.
6. Build and run it from the editor.

## Project ownership

Created projects live outside the BasilEngine source repository and consume a
built or installed BasilEngine package. Where Birds Nest may remain inside the
engine repository while it serves as the reference project.

Generated CMake files remain readable and user-editable. The editor invokes the
same build rather than maintaining a hidden parallel build definition.

## Language configuration

Projects support three language modes:

- C only
- C++ only
- Mixed C and C++

The default is mixed C11 and C++26. Each project can select its language mode
and supported C/C++ standard through simple project settings. Unsupported
compiler/standard combinations produce an actionable configuration error and
must not silently downgrade.

Public BasilEngine APIs remain C-compatible so they can be consumed naturally
from either language.

## Manifest and files

Project metadata is stored in a human-readable, versioned JSON
`.basilproject` manifest. Persistent formats carry a schema version from their
first revision and project paths are relative to the project root by default.

The implemented minimal template contains:

```text
MyGame/
|-- MyGame.basilproject
|-- CMakeLists.txt
|-- .gitignore
|-- assets/
|-- workspaces/
|   `-- Main.basilworkspace
`-- source/
    `-- main.c or main.cpp
```

Empty files or directories are included only when they serve an immediate
workflow purpose.

Project schema version 2 uses a `startupWorkspace` path and generated Projects
store `workspaces/Main.basilworkspace`. Workspace schema version 3 contains its
identity, next stable entity ID, a bounded flat entity list, and bounded
versioned component envelopes. Transform2D position and ASCII Renderable data
are the first typed components. Unknown optional component JSON is owned and
preserved; unknown required components are rejected. Parent/child relationships
are not part of the contract.

Text Sprite assets are decoded by a shared C service from normalized paths
relative to the Project root. Version 1 accepts printable ASCII with LF or CRLF
line endings, treats spaces as transparent cells, pads uneven rows, and rejects
empty visual content or dimensions beyond the documented bounds. Loads and
cache refreshes are transactional: a malformed edit cannot replace the last
known-good decoded sprite. The service does not depend on raylib or BasilEditor.

The shared C runtime interpretation service converts enabled Transform2D and
ASCII Renderable components into an owned `BAsciiDrawList`. Its glyph items use
world-space floating-point positions and retain colors, logical layer, stable
source entity ID, and deterministic entity/cell ordering. The build is
transactional, host-neutral, and applies the same anchor and transparent-space
rules for future standalone and editor presentation paths.

Generated applications call an engine-owned runtime host. It accepts an
explicit `--project <manifest>` path, otherwise checks the working directory and
then walks upward from the executable for exactly one `.basilproject`. It loads
the startup Workspace, decodes referenced Text Sprites, builds the shared draw
list, and presents it through raylib. Empty content receives a restrained
`WORKSPACE ONLINE` state; load failures are written to standard error and remain
visible in a graphical error state. `--basil-validate` performs the identical
load/interpretation path without creating a window for tooling and tests.

BasilEditor's entity picker creates visible single-glyph entities from the full
printable ASCII set, Text Sprite entities from recursively discovered `.txt`
files under `assets/`, or explicit transform-only empty entities. The Inspector
edits position, source, colors, logical layer, anchor, visibility, and
transparent-space behavior. Editor changes use shared transactional C mutation
APIs, so invalid typed values do not replace the last valid component state.

Run is a stricter operation than Build. Before starting a build, BasilEditor
validates the complete in-memory Workspace and every referenced Text Sprite,
including references on disabled or invisible entities. A failure does not save,
build, or launch; its path, line/column, entity ID, component type, and message
flow into Problems where available. A successful preflight saves through the
temporary/backup path and launches the built executable with `--project` and the
absolute manifest path as separate process arguments.

The Workspace Viewport builds the same host-neutral `BAsciiDrawList` used by the
generated runtime. It previews glyphs, Text Sprites, colors, layers, anchors,
visibility, and transparent spaces without running simulation. It adds only
editor presentation: grid/origin guides, selection outlines, pan/zoom, manual
asset refresh, and toggleable markers/name labels for enabled spatial entities
without an active renderable. Preview failure retains the last valid draw list.

Where Birds Nest provides the first maintained reference Project at
`projects/wherebirdsnest/WhereBirdsNest.basilproject`. Its startup Workspace is
ordinary schema-3 data and demonstrates a layered environment Text Sprite,
multi-line player Text Sprite, enemy glyph, and transform-only editor marker.
The older hand-coded combat arena remains a separate feasibility spike until a
later gameplay-model migration is justified.

BasilEditor loads the startup Workspace into an explicitly owned, lifecycle-safe
document held by the editor session. Loads and clones are transactional, so a
failed operation cannot partially replace the last valid document. Hierarchy
selection drives the Inspector; entities can be created, renamed, enabled or
disabled, and deleted. Save and Ctrl+S validate the complete Workspace, write a
temporary file, preserve the previous file as `.bak`, and then replace the
destination. Returning to the Project Browser with changes requires an explicit
save, discard, or cancel choice.

The editor Build command configures and builds the generated, user-editable
`CMakeLists.txt` asynchronously using the toolchain with which BasilEditor was
built. Run first performs that same build, then launches the resulting Project
as a separate development process. Build Output streams combined standard
output/error, Problems extracts compiler/linker/CMake error lines, and the menu
controls the real process through Pause, Resume, and Stop. An active build or
game must be stopped before returning to the Project Browser.

This standalone process workflow is implemented and tested. Generated runtimes
load and render the same Workspace data previewed by the editor. The game
process is not hosted in the editor Viewport; separate-window stop/build/run is
the required alpha workflow, while in-Viewport simulation remains a later
product direction.

Schema version 3 rejects unknown and duplicate structural fields. It preserves
bounded unknown optional component `data` without interpreting it, while
rejecting required component types or versions it cannot execute. Schema-1 empty
Workspaces and schema-2 entity Workspaces remain loadable and are represented as
version 3 in memory without inventing components. Their first save retains the
original file as a backup and writes the current schema.

Project schema version 1 remains loadable. Its `startupScene` path is preserved
in memory while the Project is represented using the current API. Loading does
not silently rewrite the manifest. A future explicit migration operation must
back up the version-1 file before it writes version 2 and converts any legacy
content path.

## Dependencies

Small critical dependencies, including JSON parsing/writing, may be vendored at
pinned versions. End users should not need to install these implementation
dependencies separately or troubleshoot network downloads during ordinary
project creation.

Git repository initialization is optional during project creation and remains
available from the opened-project overview. Generated projects receive a
suitable `.gitignore` regardless. BasilEditor defaults project creation to
`Documents/BasilEngine`, while allowing any location.

The overview detects Git metadata owned by the Project root, including normal
`.git` directories and worktree-style `.git` files. An initialized Project shows
`GIT REPOSITORY ACTIVE` as a disabled status control instead of offering the
operation again. A parent repository does not count as Project initialization,
and external initialization is detected while the Project remains open.

## Editor launch behavior

BasilEditor opens to the project browser when launched without arguments. It
opens a project directly when passed a `.basilproject` path. The intended
platform installers will associate that extension with BasilEditor. Each editor
window owns one project; returning to the browser closes that project view.

Older manifest schemas will be backed up and migrated when migrations exist.
Manifests from a newer unsupported schema are refused with a compatibility
error rather than being modified.

## Implementation order

1. Versioned manifest data model and validation
2. JSON parsing and writing
3. Headless empty-project generator
4. Independent generated-project build verification
5. BasilEditor application shell and project browser
6. New/Open Project interface
7. Versioned empty Workspace format and starter generation
8. Startup Workspace loading and first entity editing **Implemented**
9. Asynchronous Build, Run, Pause/Resume, and Stop controls **Implemented**

Project generation remains a testable headless engine/tooling capability rather
than logic embedded directly in an ImGui event handler.

The complete interaction, terminology, visual, code-editing, terminal, and UI
Config direction is defined in `EDITOR_EXPERIENCE.md`.
The first complete Windows alpha and its end-to-end acceptance checks are
defined in `ALPHA_PRODUCT_CONTRACT.md`.
