# BasilEngine Roadmap

This roadmap is outcome-based. A stage is complete only when its acceptance
criteria are demonstrated; creating empty subsystem directories does not count.

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

**Status:** In progress. Text-asset loading, continuous player movement, camera
tracking, wall collision, and console input capture are implemented in the
initial arena. Color layers, an enemy target, and a basic attack remain.

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
- Introduce stable scene and entity identities.
- Add the minimal component and asset-handle model proven by Milestone 1.
- Define the initial game-module boundary without promising ABI stability.

**Exit condition:** The feasibility spike uses reusable engine APIs without
Where Birds Nest concepts entering engine code.

## Milestone 3: Project and asset system

- Add a versioned `.basilproject` manifest.
- Use portable relative asset paths and stable asset identifiers.
- Store startup scene, renderer settings, and input mappings.
- Detect changed files and produce actionable load errors.

**Exit condition:** A project can be relocated, loaded, and run without changing
engine source or machine-specific paths.

## Milestone 4: Editor foundation

- Create a dockable editor application.
- Add an ASCII viewport, hierarchy, inspector, asset browser, and log console.
- Load, edit, save, play, and stop the reference scene.
- Route editor mutations through APIs compatible with future undo/redo.

**Exit condition:** The reference-game test room can be meaningfully edited and
saved without hand-editing its scene representation.

## Milestone 5: Project creation

- Create and open projects from the engine launcher or project manager.
- Generate a portable project from a maintained template.
- Track recent projects.
- Edit, build, and run the new project's initial scene.

**Exit condition:** Starting from the BasilEngine launcher, a new editable and
runnable project can be created end to end.

## Milestone 6: Game-code workflow and hot reload

- Add embedded code browsing and editing, build output, and linked diagnostics.
- Integrate language-server functionality where practical.
- Compile game code as a platform-native dynamic module.
- Load and reload through a versioned C interface.
- Preserve or reconstruct state safely and recover from failed builds/reloads.

**Exit condition:** Project code can be edited, built, and reloaded during an
editor session without restarting the editor or losing the last working module.

## Milestone 7: Action-RPG vertical slice

- Build movement, targeting, navigation, combat, health, death, and items.
- Add an ability, interaction, UI, scene transition, and save/load path.
- Author the slice through normal BasilEngine project and editor workflows.

**Exit condition:** Where Birds Nest provides a short, genuine action-RPG
encounter and exposes the next set of engine requirements.
