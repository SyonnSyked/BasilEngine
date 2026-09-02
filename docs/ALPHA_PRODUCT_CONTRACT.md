# BasilEngine Alpha Product Contract

**Contract date:** 2026-09-02  
**Target:** First complete Windows alpha  
**Status:** Accepted product direction; implementation pending  
**Reference proof:** Where Birds Nest

## Purpose

This document is the authoritative scope contract for the first BasilEngine
alpha. It converts the established product direction into one testable outcome:

> A C or C++ programmer can create a BasilEngine Project, author a small
> real-time ASCII game primarily inside BasilEditor, build and test it, export
> a self-contained game package, and run that package without BasilEditor or the
> BasilEngine source repository.

"Can I make anything with this tool?" means anything within BasilEngine's
declared first domain: smooth, real-time, two-dimensional games whose primary
visual language is ASCII glyphs and multi-character text art. The alpha is not
a promise of general-purpose 2D or 3D engine coverage.

This contract governs alpha work when older planning documents are broader or
attach later features to the same milestone. `EDITOR_EXPERIENCE.md` remains
authoritative for long-term editor experience and visual direction.

## Product boundaries

### BasilEngine owns

- Application lifecycle and the reusable standalone runtime host.
- Project, Workspace, entity, component, and asset data services.
- ASCII rendering, continuous world coordinates, input, audio, and minimal UI.
- BasilEditor authoring, code, terminal, diagnostics, build, run, and export.
- A versioned C game-module interface usable from both C and C++.
- Validation, recovery, actionable errors, and portable output assembly.

### A game Project owns

- Characters, enemies, abilities, items, dialogue, encounters, and game rules.
- Project source code and user-editable CMake files.
- Workspaces, Text Sprites, data, fonts, music, and sound effects.
- The meaning and persistence format of game-specific save data.

Where Birds Nest may prove every engine mechanism, but BasilEngine must never
contain Where Birds Nest names, rules, or special cases.

## Required end-to-end workflow

The Windows alpha must support this complete path:

1. Launch BasilEditor into the Project Browser.
2. Create an empty C, C++, or mixed Project at a chosen location.
3. Open one Project per editor window.
4. Create or edit Workspaces, entities, built-in components, Text Sprites, and
   registered Project components.
5. Edit Project C/C++ and supporting files with the built-in editor or a
   configured external editor, including a Neovim-friendly command.
6. Use one persistent Project-root terminal session when shell work is useful.
7. Save or recover changes, build the game module, and navigate actionable
   compiler or content diagnostics.
8. Run in a separate process and window; stop, rebuild, and run again for code
   changes.
9. Export a Development or Release package as a portable directory, with ZIP
   packaging available.
10. Run the export on a compatible Windows machine without the editor,
    repository, source code, or an undocumented machine-local dependency.

Separate-window stop/build/run is the alpha development-play model. In-Viewport
simulation remains the intended later default; true code hot reload is deferred.

## Alpha capability requirements

### 1. Editor reliability

- Protect unsaved Project, Workspace, and file changes on editor-controlled
  close, Project switch, build, and run paths.
- Save All before build or run; validation failure prevents the action.
- Provide undo/redo for supported mutations, entity duplication, and practical
  entity/asset search or filtering.
- Use native file/folder selection for normal open, import, and export flows.
- Save, load, import, export, select defaults for, and reset versioned JSON UI
  Configs.
- Write debounced recovery snapshots approximately ten seconds after the latest
  change. On startup, offer an explicit restore/discard choice.
- Preserve a `.bak` of the last successfully replaced persistent document.
- Never let recovery silently overwrite newer valid work.
- Use compact success notifications and visible actionable failure notices.

### 2. Assets and Text Sprites

- Stable Project-relative asset identities keep references reliable across
  supported rename and move operations.
- Project-root containment and readable errors cover missing, malformed,
  escaped, and unsupported assets.
- File-change detection observes internal and external edits.
- Transactional refresh preserves the last known-good decoded resource.
- Alpha assets include Text Sprites, JSON/data files, Project fonts, WAV/OGG
  sound effects, and OGG music.
- The built-in Text Sprite editor provides ordinary text editing, dimensions
  and grid feedback, color preview, transparent-space visualization, saving,
  and immediate authoring-preview refresh.

Rectangular painting/selection, stamping, bitmap sprites, rich text, and
embedded Text Sprite styling metadata are not alpha requirements.

### 3. Native game-code boundary

Project code builds as one platform-native dynamic module through a versioned
C-compatible interface.

- C is primary; C++ Projects call the same ABI naturally.
- Lifecycle callbacks cover initialization, update, render contribution where
  needed, and shutdown.
- Engine-owned objects cross through stable handles or bounded value types, not
  exposed internal ownership.
- Compatibility is checked before execution; errors name expected and provided
  interface versions.
- A failed build does not destroy the last valid module or editor document.
- The design permits later hot reload without implementing live replacement or
  state migration now.

Thin C++ conveniences may improve normal use but must not create a second
engine model or leak C++ ownership, exceptions, or RTTI into the C ABI.

### 4. Project-defined components

Projects may register a bounded set of custom components. BasilEditor reads a
separate, restricted, versioned metadata artifact instead of executing arbitrary
game code to draw the Inspector.

The first field kinds are boolean, signed integer, floating-point number,
string, color, entity reference, asset reference, and enumeration. Metadata
provides stable identifiers, display names, defaults, relevant bounds/choices,
and Inspector grouping. Data remains human-readable and versioned in Workspace
JSON. Unknown optional data is preserved; unknown required behavior is rejected.
This is not general reflection, arbitrary native Inspector code, or plugins.

### 5. Programming workflow

The built-in code editor is the default and provides:

- Multiple tabs and a Project source tree.
- Create, rename, delete, open, save, Save All, and dirty-file protection.
- Line numbers and highlighting for C, C++, CMake, JSON, and text.
- Find/replace, go to line, indentation, bracket matching, and undo/redo.
- Build/run shortcuts and diagnostic navigation to file and line.
- Open in external editor and reveal in terminal or file browser.

Semantic completion, refactoring, a debugger, Git UI, and bundled AI assistance
are deferred. External-editor settings remain a simple command with documented
file, line, and column placeholders.

The alpha Terminal is one persistent, expandable panel with configurable shell,
Project-root working directory, input/output, copy/paste, clear, and restart.
Windows PowerShell is the Windows default; the user's login shell is the later
macOS/Linux default. Multiple tabs and custom terminal emulation are deferred.

### 6. Runtime services

- Real-time delta-time update and smooth world-space motion independent of
  glyph cells.
- Glyph and multi-character Text Sprite presentation.
- Safe entity lookup and access to built-in and registered components.
- Named input actions with configurable keyboard/mouse bindings, edge/held
  states, and focus suppression. Gamepads are deferred.
- One active Workspace and a replacement request API. A failed load reports the
  error and leaves the current Workspace running.
- Collision-facing APIs sufficient for movement, attacks, triggers, and
  interaction without promising a general physics engine.
- Minimal ASCII UI: labels, boxes, buttons, simple layout, window anchoring,
  HUD, dialogue, and selectable choices, operable by keyboard and mouse.
- WAV/OGG sound effects and OGG music with master/category volume, play, stop,
  pause, loop, and basic fades.

Additive Workspaces, gamepads, advanced physics/navigation, animation tooling,
and a visual audio editor are deferred.

### 7. Build and export

BasilEditor uses the visible, user-editable Project CMake files and never keeps
a hidden competing build description.

- Development exports retain useful developer diagnostics.
- Release exports optimize and omit unnecessary development material.
- Release diagnostic logs can be enabled by a documented command-line flag.
- Both assemble the executable, module, runtime libraries, manifest, Workspaces,
  and referenced assets into a relocatable directory.
- Validation catches missing or escaped dependencies before reporting success.
- Optional ZIP output packages that same directory.
- JSON and Text Sprite assets remain ordinary readable files.

Tests include a destination containing spaces and launch outside the repository.
A clean VM or isolated environment must expose accidental PATH, source-tree, or
machine-local dependency assumptions.

## Where Birds Nest alpha proof

The mandatory end-to-end proof is approximately ten to twenty minutes and has:

- At least two connected Workspaces.
- Smooth movement and collision.
- Two meaningfully different enemy types.
- A basic attack and one ability.
- Health, damage, death, and restart.
- One NPC or contextual interaction.
- One obtainable or usable item.
- A Workspace transition.
- A functional HUD.
- Dialogue with at least one selectable choice.
- Sound effects and music.
- A clear completion state.

It must be authored and programmed through supported alpha workflows, then
built, run, and exported from BasilEditor. This validates engine mechanisms,
not final balance, content volume, or the complete game design. Deferred design
questions remain in `OPEN_DESIGN_QUESTIONS.md` until a bounded step needs them.

## Quality and engineering rules

- Persistent formats are versioned from revision one.
- Reads are bounded, validation explicit, and failed mutations preserve valid
  state.
- Editor and runtime share Project, Workspace, asset, and interpretation code.
- GUI, shortcuts, terminal actions, and automation call shared services.
- Cross-platform intent stays behind narrow interfaces while Windows alone
  blocks alpha completion.
- Every step gets practical automated checks, a manual exit check where needed,
  documentation updates, and a reviewable commit.
- New abstractions serve a visible alpha workflow or proven safety requirement.
- A need for a deferred subsystem triggers scope review, not silent expansion.

## Explicit alpha exclusions

- In-Viewport gameplay and embedded simulation.
- Code hot reload or live state migration.
- Additive/concurrent Workspaces and gamepad input.
- General ECS, unrestricted reflection, plugins, or marketplace.
- Semantic IDE completion, refactoring, debugger, or integrated Git client.
- Multiple terminal tabs or a custom terminal emulator.
- Advanced Text Sprite painting, animation, bitmap-sprite, tilemap, or 3D tools.
- General physics/navigation, networking, multiplayer, or game-save framework.
- Linux/macOS certification, installers, or public distribution.
- Production content scale and post-alpha API/ABI stability.

## Delivery stages and exit checks

Every stage requires focused tests, the applicable regression suite, documented
manual review, documentation updates, and a reviewable commit.

### Stage 1: Alpha contract and baseline

- This contract agrees with accepted product decisions and the charter.
- README, roadmap, architecture, and scope status contain no contradictory alpha
  requirements.
- Current repository tests establish the implementation baseline.

### Stage 2: Editor reliability

**Implementation status:** Complete; Windows interactive checklist pending.

- Supported mutations participate in undo/redo and close protection.
- Recovery survives forced termination without silently overwriting files.
- UI Configs and native file dialogs complete their agreed workflows.

### Stage 3: Asset foundation

**Implementation status:** Complete; Windows interactive checklist pending.

- Identity, watching, transactional refresh, and diagnostics work for each alpha
  asset kind.
- A Text Sprite can be created, edited, previewed, externally changed, moved,
  and recovered without breaking authored references.

### Stage 4: Native game-code boundary

**Implementation status:** Complete; Windows interactive checklist pending.

- C, C++, and mixed Projects execute through the versioned module interface.
- Custom metadata/data round-trip without executing Project code in Inspector.
- Compatibility and failed-build paths preserve the last valid state/artifact.

### Stage 5: Integrated programming workflow

- A new Project can be programmed, built, diagnosed, and rerun using only the
  editor.
- External-editor and terminal routes work without a competing workflow.

### Stage 6: Playable runtime services

- A test Project proves input rebinding, collision-facing behavior, custom
  components, safe Workspace replacement, UI/dialogue, and audio.
- Each service is usable through the public C API and from a C++ Project.

### Stage 7: Export pipeline

- Development and Release packages validate and run after relocation.
- ZIP reproduces the directory package; Release logging is explicitly opt-in.

### Stage 8: Where Birds Nest alpha

- The complete proof checklist is authored through supported workflows.
- It builds, runs, and exports from BasilEditor without private fixes.

### Stage 9: Independent verification and audit

- The Release export runs in a clean-machine-style Windows environment.
- Playing requires no source repository, editor, compiler, or undocumented
  dependency.
- The full suite passes and the manual checklist and scope audit are recorded.

## Definition of alpha complete

Alpha is complete only when Stage 9 passes. Panels and APIs alone are not
sufficient. The deciding artifact is an independently runnable Where Birds Nest
Release package produced through the normal BasilEditor workflow, accompanied
by passing automated evidence and a recorded manual verification checklist.
