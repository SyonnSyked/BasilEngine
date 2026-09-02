# BasilEditor Visual Foundation Closeout

**Status:** Complete

This document closes the BasilEditor visual-foundation finishing pass. It is a
record of what is present and verified, not a claim that the full editor
foundation milestone is complete. The product rules and future interaction
model remain defined by `EDITOR_EXPERIENCE.md`.

## Delivered scope

- A centralized ImGui theme module owns the semantic dark NetRunner palette,
  electric-cyan primary accent, restrained violet secondary accent, geometry,
  spacing, and non-compounding interface scaling.
- JetBrains Mono Regular and Bold v2.304 are bundled with their SIL Open Font
  License, provenance, and hashes. Missing fonts fall back safely and report an
  explanation without preventing launch.
- Global, versioned JSON preferences persist interface-scale presets of 100%,
  115%, 135%, 150%, and 175%, with 135% as the default. Invalid or unsupported
  preference data falls back safely.
- The Project Browser and Project Overview use the shared visual system. The
  overview deliberately presents a placeholder Viewport instead of implying
  that Workspace editing already exists.
- A geometric circuit-leaf application mark is maintained as an SVG master
  with reviewed PNG sizes and a multi-resolution Windows ICO. Windows builds
  embed the executable icon, and the running window loads the PNG identity.
- Git initialization remains optional. The overview detects a `.git` directory
  or worktree-style `.git` file owned by the Project root, ignores only-parent
  repositories, refreshes while open, and replaces the action with a disabled
  `GIT REPOSITORY ACTIVE` state after initialization.
- BasilEditor has a top-level C++ exception boundary so unexpected standard
  exceptions become visible launch errors instead of silent termination.

## Verification record

The finishing pass was verified on the current Windows UCRT64 development
environment using both runtime-only and editor-enabled clean build trees.

- Runtime-only configuration and build: passed; 7 of 7 tests passed.
- Editor-enabled configuration and build: passed; 9 of 9 tests passed.
- Generated C-only, C++-only, and mixed C/C++ Projects: configured and built.
- Project-browser launch, direct `.basilproject` launch, and missing-font
  fallback launch: passed.
- Preferences and Project-root Git detection: covered by automated tests.
- Static analysis: selected cppcheck and clang-tidy checks completed cleanly
  for maintained engine/editor code; vendored ImGui sources were excluded.
- Packaging checks: bundled font/icon copies match their sources, PNG
  dimensions and ICO entries were inspected, the Windows resource section is
  present, and executable imports use system DLLs rather than requiring the
  GCC runtime beside the application.

This is comprehensive evidence for the tested Windows toolchain. Native macOS
and Linux builds and packaging have not yet been executed, so cross-platform
support remains an engineering target rather than a verified release claim.

## Intentional boundaries

This pass did not implement dockable content panels, Workspace persistence or
editing, UI Config persistence, native file dialogs, operating-system file
association, build/run controls, the built-in code editor, terminal hosting,
or hot reload. The macOS ICNS and distinct `.basilproject` and Where Birds Nest
icons also remain future packaging work.

These exclusions are deliberate. Visual polish now has a stable shared base,
but it must not be mistaken for functional editing capability.

## Next milestone handoff

The initial Workspace-format and dockable-shell items below were completed after
this closeout. The remaining sequence still defines the editor-foundation
handoff:

1. Define a versioned Workspace model, serialization schema, and extension,
   including compatibility with the legacy `scenes/` directory and
   `startupScene` manifest field. **Completed:** the empty format and load path
   exist; backup-aware on-disk migration remains part of future editing work.
2. Add the dockable editor shell and the maintained slim default UI Config,
   keeping UI Config state separate from Project content. **Completed:** the
   shell and resettable default model exist; portable JSON persistence remains.
3. Introduce Workspace Hierarchy, Inspector, Asset Browser, Console, and
   Viewport panel scaffolds backed by shared services rather than local widget
   state. **Scaffolds completed:** panels consume shared Project/UI Config state
   where it exists; build output and problems are now connected, while terminal
   hosting remains separate.
4. Load, create, edit, and save one empty Workspace reliably before additive
   Workspace behavior or development play is attempted. **Completed:** the
   generated startup Workspace supports flat entity editing and safe save.
5. Add build/run integration only after the Project and Workspace paths are
   represented consistently in both editor and runtime code. **Partially
   completed:** asynchronous standalone build/run and process controls exist;
   runtime Workspace consumption remains the next integration boundary.

The `.basilworkspace` extension and empty-file schema are now contracts.
Entity/component serialization and additive semantics still require explicit
decisions before those capabilities are implemented.
