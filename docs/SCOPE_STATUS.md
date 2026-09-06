# BasilEngine Scope and Course Audit

**Audit date:** 2026-09-02  
**Project phase:** Alpha Stage 3 asset foundation closeout
**Overall verdict:** On course; stable asset identity and editing are active

BasilEngine still matches the product charter. The repository has not drifted
into game-specific engine code, a speculative ECS, or an oversized editor.
The earlier gap between editor-authored data and runtime rendering is now
closed. Further work can build on one shared Project, Workspace, Text Sprite,
and draw-list path without expanding into a speculative ECS or oversized IDE.

The accepted final-stretch boundary is now recorded in
`ALPHA_PRODUCT_CONTRACT.md`. It requires a complete create/author/program/build/
run/export loop and an independently runnable Where Birds Nest proof. It keeps
the alpha achievable by explicitly deferring embedded gameplay, code hot reload,
semantic IDE features, gamepads, additive Workspaces, general-purpose engine
scope, and non-Windows certification.

This document is the current status checkpoint. The charter and architecture
remain the source of truth for product intent; the roadmap remains the ordered
plan; and the Alpha Product Contract governs first-alpha inclusion and exit
criteria.

## Verification snapshot

At this checkpoint, all 11 runtime/core tests and all 16 editor-enabled tests
pass on the Windows development machine. These suites include Project and
Workspace persistence, recent Projects, interactive Project creation, generated
Project compilation, combat logic, ASCII canvas behavior, input, editor
preferences, Git detection, UI Config validation, Workspace sessions, and the
asynchronous build/run service. This is strong regression evidence, but it is
not a substitute for native macOS/Linux testing or the outstanding play-feel
review.

## North-star comparison

| Intended outcome | Current evidence | Status | Remaining gap |
| --- | --- | --- | --- |
| BasilEngine is reusable; Where Birds Nest is its proving project | WBN uses `BGame.h` with Basil-owned bootstrap/glue and integrates collision, input, UI, and Workspace replacement | Aligned through the pre-audio runtime gate | Complete bounded audio and later alpha proof work |
| ASCII and text art are first-class | Strict decoding/cache, stable asset registry, change detection, bounded Text Sprite editor, deterministic interpretation, standalone rendering, and reference room exist | Shared authoring/runtime foundation proven | Exercise the visible Stage 3 checklist and later export relocation |
| Smooth real-time action-RPG play | Delta-time movement, camera, collision, input routing, a target, attack cooldown, damage, and death exist | Feasibility spike complete in breadth; feel unvalidated | Conduct play-feel validation before treating combat APIs as stable |
| C-first runtime with narrow C++ use | Runtime and `BGame.h` are C11-compatible; generated C, C++, and mixed Projects build through the same boundary | Aligned | Preserve the C boundary as remaining services are introduced |
| Windows, macOS, and Linux | Cross-platform code paths and CMake structure exist | Not fully verified | Windows is the active verified platform; macOS/Linux need native configure, build, test, and process-control verification |
| Medium-ambition, practical editor | Browser, dock shell, glyph/Text Sprite/empty entity authoring, component Inspector, shared-data Viewport preview, build/run controls, diagnostics, and preferences exist | Strong foundation | Development-play hosting and later code workflow remain |
| New empty Projects work without manual engine setup | Generator creates manifests, editable CMake, source, assets, and a starter Workspace; editor authors, previews, validates, saves, builds, and runs them | End-to-end foundation proven | Native platform/toolchain packaging remains before a public-quality workflow |
| Editor and runtime use the same data and APIs | Project, Workspace, Text Sprite, draw-list, collision, and replacement services are shared; WBN consumes the public runtime API | Aligned for the pre-audio slice | Audio remains the next runtime service |
| Built-in coding and terminal workflow, with Neovim first-class | Build Output and Problems plumbing exist; terminal and code-editor panels are planned | Deliberately deferred | Implement only after the runtime/project boundary is stable; avoid building an IDE or terminal emulator prematurely |
| Mature NetRunner visual identity | Central theme, electric-cyan/restrained-violet palette, bundled JetBrains Mono, scale preferences, dock layout, and application icon exist | Aligned | Apply the system consistently to future functional panels; avoid decorative expansion |
| Hot-loadable game development | Versioned native modules and last-valid artifact preservation exist; hot reload/state migration do not | Deliberately deferred | Not an alpha requirement |

## Current functional inventory

### Runtime and reference-game proof

- Application/engine initialization, run loop, shutdown, time, and frame
  management.
- Named keyboard actions and gameplay-input suppression while the console owns
  input.
- In-memory diagnostics and a developer console overlay.
- Layered ASCII rendering with per-cell foreground/background colors.
- Plain-text asset loading and runtime glyph editing.
- Bounded Text Sprite decoding with LF/CRLF support, transparent-space grids,
  root-contained paths, structured diagnostics, and last-known-good caching.
- Deterministic host-neutral draw lists with fractional anchor placement, layer
  ordering, colors, transparency, visibility, and stable source entity IDs.
- Generated runtime discovery from explicit manifests, working directories, or
  executable ancestry; startup-Workspace loading; draw-list presentation; and
  stable empty/error states.
- Editor creation for printable glyph, Project Text Sprite, and transform-only
  empty entities, plus focused validated Transform2D/ASCII Renderable controls.
- Run preflight across all referenced Text Sprites, structured Problems output,
  safe autosave, and an absolute `--project` child-process launch contract.
- Shared-data Viewport rendering with pan/zoom, grid/origin guides, selection
  outlines, and toggleable editor-only markers and labels for empty entities.
- Smooth movement, camera tracking, arena collision, and corrected contact on
  all wall sides.
- A damageable target, range-checked basic attack, cooldown, visual feedback,
  health, and death state in Where Birds Nest.
- An editor-openable Where Birds Nest Project whose schema-4 rooms integrate
  stable AssetRefs, Collider2D walls/trigger interaction, modal HUD/dialogue,
  safe Workspace replacement, generation detection, and handle reacquisition.

### Project and persistence foundation

- Versioned JSON `.basilproject` manifests with strict validation.
- C-only, C++-only, and mixed language modes; mixed C11/C++26 defaults.
- A headless and interactive Project generator that creates an intentionally
  minimal, editable CMake project.
- Versioned JSON `.basilworkspace` files with stable Workspace and entity IDs.
- A bounded, flat entity model supporting name and enabled state.
- Safe Workspace saves using a temporary file and previous-file backup.
- Legacy Project/Workspace loading without silent disk migration.
- Recent Projects, configurable/default creation location, optional Git
  initialization, and Project-root Git detection.

### BasilEditor

- Project Browser with New, Open, and Recent workflows; one Project per window.
- Direct `.basilproject` command-line opening, ready for later installer-level
  file association.
- Dockable editor shell, stable panel registry, command menus, and resettable
  in-memory default UI Config.
- Project Details, Workspace Viewport, Hierarchy, Inspector, Assets, Console,
  Build Output, Problems, and Terminal panel foundations.
- Startup Workspace loading and dirty tracking.
- Entity create/select/rename/enable/delete and save/Ctrl+S.
- Unsaved-change protection when returning to the Project Browser.
- Asynchronous configure/build/run with streamed output and extracted compiler,
  linker, and CMake problems.
- Pause, resume, stop, and child-process cleanup for the standalone development
  process.
- Central NetRunner theme, bundled font, persistent UI scaling, and branded
  application icon.

## Milestone scorecard

| Milestone | Assessment | What prevents completion |
| --- | --- | --- |
| 0 — Reliable foundation | Mostly achieved on the current Windows development machine | A clean-checkout dependency story and native macOS/Linux verification are still required |
| 1 — Gameplay feasibility spike | Mechanically implemented; still in progress | Responsiveness and ASCII combat feel need an intentional play-test decision |
| 2 — Reusable runtime model | In progress with its first coherent slice complete | Explicit ownership, minimal components, asset identity/decoding, and shared rendering exist; the game-module boundary remains |
| 3 — Project and asset system | Substantially achieved on Windows | Stable IDs, focused watching, Text Sprite authoring, diagnostics, and relocation tests exist; later runtime asset kinds still need their consumers |
| 4 — Editor foundation | In progress and healthy | Component authoring and shared-data Viewport preview are active; gameplay remains in the controlled external process |
| 5 — Project creation | Exit condition achieved on Windows | A new Project can be created, opened, authored, previewed, saved, built, and run end to end; native macOS/Linux and distribution verification remain |
| 6 — Alpha game-code workflow | Supporting build/diagnostic plumbing only | Code editor, external-editor service, single terminal session, and versioned module boundary remain |
| 7 — Action-RPG vertical slice | Not started as a production slice | The current arena is a feasibility proof, not a Workspace-authored vertical slice |
| 8 — Windows alpha export and proof | Contract accepted; implementation pending | Reliability, asset, programming, runtime-service, export, proof-game, and independent verification stages remain |

Milestones overlap in implementation because the editor needed Project creation
to become useful. Their exit conditions must not be declared complete out of
order merely because a button or panel exists.

## Highest-priority gaps and risks

1. **Portability is promised but not demonstrated.** Build-service defaults
   currently reflect the toolchain and paths used to build BasilEditor. That is
   acceptable for this development milestone, not an install/distribution
   solution.
2. **Dependency reproducibility.** Local raylib/ImGui integration works on the
   development machine, but clean-checkout and cross-platform provisioning need
   a deliberate policy before Milestone 0 can be considered fully closed.
3. **Asset identity has a deliberate bridge boundary.** The Project registry is
   authoritative and Workspace schema 4 stores stable AssetRefs. Schema 3
   remains readable as the legacy path-reference format; move repair is
   transactional.
4. **Prototype limits may be mistaken for permanent architecture.** The flat
   512-entity Workspace is a useful bounded first format, not a reason to build
   a general ECS now and not a promise of final scale.
5. **Shutdown safety is incomplete.** Project-browser transitions protect dirty
   data, but native window/application shutdown still needs equivalent
   unsaved-change handling.
6. **Gameplay feel remains unvalidated.** The original combat spike has useful
   mechanics, but its responsiveness should be deliberately play-tested before
   its APIs shape the reusable gameplay model.
7. **Feature-surface temptation.** UI Config files, native dialogs, terminal
   hosting, and bounded code editing now serve the alpha workflow. Semantic IDE
   features, a debugger, custom terminal emulation, and hot reload do not.

## Scope guardrails for a two-person team

### Now

- One active Project per editor window.
- One active, flat Workspace with stable entities.
- Execute the ordered stages in `ALPHA_PRODUCT_CONTRACT.md`.
- Begin with editor reliability; do not couple recovery, UI Config persistence,
  and native dialogs to unrelated gameplay systems.
- Preserve the proven shared Project/Workspace/Text Sprite rendering path.
- Keep every stage independently testable and reviewable.

### Immediate next work

- Complete remaining interactive verification checklists and continue the
  bounded Stage 6 runtime services after input, collision, Workspace
  replacement, and minimal ASCII UI.

### Later

- Built-in code editing, external-editor commands, and one terminal session.
- Alpha runtime services, export, and the Where Birds Nest proof.
- After alpha: safe hot reload, in-Viewport play, semantic IDE features, and a
  custom terminal emulator.
- Additive Workspaces, richer components, and hierarchy when a demonstrated
  use case requires them.
- Installers/file association and full three-platform release verification.

### Not now

- A general-purpose ECS or reflection framework.
- A plugin marketplace, package manager, cloud service, collaboration layer, or
  networking stack.
- A custom debugger, full IDE replacement, or terminal emulator.
- Multiple rendering backends, mobile/console support, or AAA asset pipelines.
- Broad action-RPG content systems before one editor-authored room runs through
  the reusable engine path.

## Completed development slice

The completed slice is **Runtime Workspace Bridge**. Its acceptance result was:
create or edit an entity in BasilEditor, press Run, and see that entity rendered
by the generated standalone application using shared engine APIs.

The concrete schema, ownership, Text Sprite, diagnostics, migration, testing,
and implementation contract is defined in `RUNTIME_WORKSPACE_BRIDGE.md`.

The slice introduced:

1. A C runtime API that loads a Project and its startup Workspace.
2. Minimal transform and ASCII-renderable component data with validation.
3. Mapping from persisted Workspace entities into runtime-owned state.
4. Generated entry-point code that runs that shared path.
5. Tests proving load, validation, render mapping, and the generated workflow.

The completed Runtime Workspace Bridge intentionally excluded the systems above.
Subsequent alpha stages have now added editor reliability, the bounded asset
foundation, and the native game-module/restricted component metadata boundary
without revising its proven data path. The next bounded acceptance result is an
integrated programming workflow, not hot reload or a generalized ECS.

## Recurring scope-check process

Run this audit:

- before beginning a new subsystem or roadmap milestone;
- after each coherent slice, normally three to five reviewable commits;
- before changing a persistent schema or public engine boundary;
- whenever a convenient feature does not directly support the current exit
  criterion; and
- at minimum once per active development month.

Each checkpoint updates this file and answers:

1. What user-visible outcome now works end to end?
2. Which claims are tested, and on which platforms?
3. What remains scaffolded, partial, or unverified?
4. Did any game-specific policy enter BasilEngine?
5. Did the slice add a reusable mechanism or speculative machinery?
6. What debt was accepted, and when must it be repaid?
7. What is the single next acceptance result?
8. What attractive work remains explicitly out of scope?

A proposed slice should normally add no more than one new subsystem boundary,
one persistent-format revision, and one primary user-visible workflow. If it
requires two unfinished future systems to be useful, it should be split or
deferred.

## Decisions that remain settled

- Where Birds Nest is the reference Project, not BasilEngine itself.
- Real-time, smooth simulation remains independent of glyph-cell presentation.
- C11 is the runtime default; C++ is used where integrations require it.
- Workspace and UI Config remain distinct canonical terms.
- The built-in code editor is the eventual default; Neovim and external tools
  remain first-class options.
- Persistent formats are versioned, human-readable JSON; generated CMake stays
  user-editable.
- The default editor experience remains restrained, dark, cyberpunk, scalable,
  dockable, and Viewport-dominant.
- Deferred gameplay questions remain deferred until the shared engine workflow
  can exercise their answers.
