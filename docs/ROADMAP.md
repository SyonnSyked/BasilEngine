# BasilEngine Roadmap

This roadmap is outcome-based. A stage is complete only when its acceptance
criteria are demonstrated; creating empty subsystem directories does not count.
The current cross-milestone assessment and two-person-team guardrails are
maintained in `SCOPE_STATUS.md`.

## Milestone 0: Reliable foundation

- Maintain a clean milestone branch and reviewable commits.
- Document the product direction, architecture, and build procedure.
- Enable strong compiler warnings.
- Establish a headless test target.
- Remove machine-specific dependency assumptions from the normal build path.
- Verify launch, console input, supplied window configuration, and shutdown.

**Exit condition:** A clean checkout can be configured from documented
dependencies, built, and tested consistently on the supported development
platform.

## Milestone 1: Gameplay feasibility spike

**Status:** In progress. Text-asset loading, per-cell foreground/background
colors, continuous player movement, camera tracking, wall collision, and console
input capture are implemented in the initial arena. A damageable enemy target,
range-checked basic attack, cooldown, feedback, and death state are also present.
The spike now awaits play-feel validation before its runtime model is stabilized.

- Load an environment from a text asset.
- Render colored glyphs in world space.
- Move a player smoothly using delta time.
- Track the player with a camera.
- Resolve collision against a small test arena.
- Add one target and one basic attack.
- Route input so editor or console focus suppresses gameplay actions.

**Exit condition:** The small arena demonstrates that continuous action combat
can feel responsive while retaining the intended ASCII identity.

## Milestone 2: Reusable runtime model

- Formalize subsystem ownership through explicit engine state.
- Separate world, glyph-cell, and screen coordinates.
- Introduce stable Workspace and entity identities.
- Add the minimal component and asset-handle model proven by Milestone 1.
- Define the initial game-module boundary without promising ABI stability.

**Exit condition:** The feasibility spike uses reusable engine APIs without
Where Birds Nest concepts entering engine code.

Gameplay design questions that are not required for the initial editor and
project workflow are tracked in `OPEN_DESIGN_QUESTIONS.md` and intentionally
deferred.

## Milestone 3: Project and asset system

**Status:** In progress. Versioned JSON manifests, validation, configurable
C/C++ language rules, a headless empty-project generator, independent build
verification, and the first editor-facing project browser are implemented.
Asset identifiers, startup-Workspace runtime loading, native file dialogs, and
file-change detection remain. A bounded project-relative Text Sprite decoder and
transactional last-known-good cache are now implemented as the first shared
asset service.

- Add a versioned `.basilproject` manifest.
- Use portable relative asset paths and stable asset identifiers.
- Store startup Workspace, renderer settings, and input mappings.
- Detect changed files and produce actionable load errors.

**Exit condition:** A project can be relocated, loaded, and run without changing
engine source or machine-specific paths.

## Milestone 4: Editor foundation

- **Status:** In progress. BasilEditor now has a graphical project browser,
  New/Open/Recent workflows, an opened-project overview, and optional Git
  initialization. Its current screens use the centralized NetRunner theme,
  bundled typography, persistent scale preferences, application branding, and
  Project-root Git-state detection. The visual-foundation finishing pass is
  complete and recorded in `EDITOR_VISUAL_FOUNDATION.md`; dockable content
  panels and actual Workspace editing remain. The versioned empty-Workspace
  format, `.basilworkspace` extension, starter generation, and legacy manifest
  load path are implemented. The genuine ImGui dockspace, stable panel registry,
  main command surface, and resettable slim default UI Config are implemented;
  portable UI Config persistence is not. All planned foundation panel windows
  now exist. The startup Workspace loads into an editor session; flat entities
  can be created, selected, renamed, enabled/disabled, deleted, and safely saved.
  Asynchronous CMake Build/Run, output capture, problem extraction, and native
  Pause/Resume/Stop process control are implemented. Lifecycle-owned Workspace
  documents and the first versioned Transform2D/ASCII Renderable component
  schema and the shared Text Sprite loading/cache service are implemented.
  Shared host-neutral draw-list interpretation and generated-runtime Project/
  Workspace discovery, loading, and presentation are also implemented. Focused
  editor component authoring, validated explicit-manifest Run, and shared-data
  Viewport preview are active; in-Viewport play, source navigation, and terminal
  services remain.
- Create a dockable editor application. **Implemented.**
- Add an ASCII viewport, hierarchy, inspector, asset browser, and log console.
  **Panel scaffolds implemented; editing and rendering behavior remains.**
- Load, edit, save, play, and stop the reference Workspace. **Startup loading,
  first entity editing, safe save, and standalone process controls implemented;
  runtime Workspace consumption and in-Viewport play remain.**
- Add a slim default UI Config and portable JSON UI Config persistence.
  **Default model implemented; persistence remains.**
- Establish the restrained cyberpunk visual system and bundled monospace font.
- Route editor mutations through APIs compatible with future undo/redo.

**Exit condition:** The reference-game test room can be meaningfully edited and
saved without hand-editing its Workspace representation.

The active implementation slice now has a complete shared path from generated
runtime Project discovery through Workspace/Text Sprite interpretation and
standalone presentation. Editor component authoring, the explicit Run contract,
and Viewport reuse remain. Its accepted contract and progress are recorded in
`RUNTIME_WORKSPACE_BRIDGE.md`.

## Milestone 5: Project creation

- Create and open projects from the engine launcher or project manager.
- Generate a portable project from a maintained template.
- Track recent projects.
- Edit, build, and run the new project's initial Workspace.

**Exit condition:** Starting from the BasilEngine launcher, a new editable and
runnable project can be created end to end.

## Milestone 6: Game-code workflow and hot reload

- Add embedded code browsing and editing, build output, and linked diagnostics.
- Keep the built-in editor as the default while supporting simple,
  configurable external-editor commands, including Neovim.
- Provide an expandable terminal panel with platform-appropriate shell defaults.
- Integrate language-server functionality where practical.
- Compile game code as a platform-native dynamic module.
- Load and reload through a versioned C interface.
- Preserve or reconstruct state safely and recover from failed builds/reloads.

**Exit condition:** Project code can be edited, built, and reloaded during an
editor session without restarting the editor or losing the last working module.

## Milestone 7: Action-RPG vertical slice

- Build movement, targeting, navigation, combat, health, death, and items.
- Add an ability, interaction, UI, Workspace transition, and save/load path.
- Author the slice through normal BasilEngine project and editor workflows.

**Exit condition:** Where Birds Nest provides a short, genuine action-RPG
encounter and exposes the next set of engine requirements.
