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
scaffolds, including a read-only top-level asset listing. Native file dialogs
and editable Workspace content remain future work.

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
store `workspaces/Main.basilworkspace`. The Workspace file is versioned JSON and
currently describes an empty, named content unit; entity content is reserved
until its model is defined rather than accepting data the engine cannot retain.

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
8. Starter Workspace and asset editing
9. Build and Run controls

Project generation remains a testable headless engine/tooling capability rather
than logic embedded directly in an ImGui event handler.

The complete interaction, terminology, visual, code-editing, terminal, and UI
Config direction is defined in `EDITOR_EXPERIENCE.md`.
