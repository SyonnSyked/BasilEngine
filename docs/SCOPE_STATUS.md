# BasilEngine Scope and Course Audit

**Audit date:** 2026-09-02  
**Project phase:** Editor/project foundation approaching its runtime-integration
boundary  
**Overall verdict:** On course, with one deliberate correction required

BasilEngine still matches the product charter. The repository has not drifted
into game-specific engine code, a speculative ECS, or an oversized editor.
However, editor and project-management features have advanced ahead of the
reusable runtime data model. The next development slice must connect Projects,
Workspaces, and runtime rendering before more editor surface area is added.

This document is the current status checkpoint. The charter and architecture
remain the source of truth for product intent; the roadmap remains the ordered
plan.

## Verification snapshot

At this checkpoint, all 9 runtime/core tests and all 14 editor-enabled tests
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
| BasilEngine is reusable; Where Birds Nest is its proving project | Engine and reference game are separate, and documented architecture forbids game concepts in engine code | Aligned | Move the feasibility arena onto the same Project/Workspace runtime path used by generated projects |
| ASCII and text art are first-class | Layered glyph rendering, a strict shared Text Sprite decoder/cache, and versioned Transform2D/ASCII Renderable Workspace data exist | Foundation proven | Interpret Text Sprites through the component path and introduce stable asset identities later |
| Smooth real-time action-RPG play | Delta-time movement, camera, collision, input routing, a target, attack cooldown, damage, and death exist | Feasibility spike complete in breadth; feel unvalidated | Conduct play-feel validation before treating combat APIs as stable |
| C-first runtime with narrow C++ use | Runtime and public-facing systems are C11; BasilEditor uses C++ for ImGui integration | Aligned | Preserve the C boundary when component and future game-module APIs are introduced |
| Windows, macOS, and Linux | Cross-platform code paths and CMake structure exist | Not fully verified | Windows is the active verified platform; macOS/Linux need native configure, build, test, and process-control verification |
| Medium-ambition, practical editor | Browser, dock shell, core panels, Workspace entity editing, build/run controls, diagnostics, and preferences exist | Strong foundation | Component controls, Viewport content rendering, runtime consumption, and later code workflow remain |
| New empty Projects work without manual engine setup | Generator creates manifests, editable CMake, source, assets, and a starter Workspace; editor can create, open, edit, build, and launch them | Partial end-to-end | The launched program does not yet consume or render its Workspace, so “runnable Project” is not fully achieved |
| Editor and runtime use the same data and APIs | Project and Workspace parsing/editing are shared engine-side capabilities | Partial | Standalone runtime still bypasses Workspace entity data; this is the highest-priority mismatch |
| Built-in coding and terminal workflow, with Neovim first-class | Build Output and Problems plumbing exist; terminal and code-editor panels are planned | Deliberately deferred | Implement only after the runtime/project boundary is stable; avoid building an IDE or terminal emulator prematurely |
| Mature NetRunner visual identity | Central theme, electric-cyan/restrained-violet palette, bundled JetBrains Mono, scale preferences, dock layout, and application icon exist | Aligned | Apply the system consistently to future functional panels; avoid decorative expansion |
| Hot-loadable game development | Architecture records a future versioned C module boundary and failure recovery | Deliberately deferred | Requires stable runtime ownership, handles, serialization, and module lifecycle first |

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
- Smooth movement, camera tracking, arena collision, and corrected contact on
  all wall sides.
- A damageable target, range-checked basic attack, cooldown, visual feedback,
  health, and death state in Where Birds Nest.

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
| 2 — Reusable runtime model | In progress | Explicit Workspace ownership and minimal serialized components exist; asset handles and the Project/Workspace-driven runtime path are missing |
| 3 — Project and asset system | In progress | Stable asset identifiers, file-change detection, actionable asset errors, and relocation across machines remain |
| 4 — Editor foundation | In progress and healthy | The Viewport does not render editable content, component controls are absent, and runtime play is external |
| 5 — Project creation | Workflow shell implemented, outcome incomplete | A generated Project builds and launches, but its runtime does not yet render editor-authored Workspace data |
| 6 — Code workflow and hot reload | Supporting build/diagnostic plumbing only | Code editor, external-editor service, real terminal hosting, module boundary, and reload safety are intentionally deferred |
| 7 — Action-RPG vertical slice | Not started as a production slice | The current arena is a feasibility proof, not a Workspace-authored vertical slice |

Milestones overlap in implementation because the editor needed Project creation
to become useful. Their exit conditions must not be declared complete out of
order merely because a button or panel exists.

## Highest-priority gaps and risks

1. **Runtime/Workspace disconnect.** The editor can author entity records and
   launch a process, but that process does not consume those records. This is
   the main product gap and the next course correction.
2. **Runtime model lag.** Stable IDs, ownership, and minimal Transform2D/ASCII
   Renderable persistence now exist, but runtime interpretation and asset handles
   do not. They should be added only in the smallest form needed by the
   reference room.
3. **Portability is promised but not demonstrated.** Build-service defaults
   currently reflect the toolchain and paths used to build BasilEditor. That is
   acceptable for this development milestone, not an install/distribution
   solution.
4. **Dependency reproducibility.** Local raylib/ImGui integration works on the
   development machine, but clean-checkout and cross-platform provisioning need
   a deliberate policy before Milestone 0 can be considered fully closed.
5. **Prototype limits may be mistaken for permanent architecture.** The flat
   512-entity Workspace is a useful bounded first format, not a reason to build
   a general ECS now and not a promise of final scale.
6. **Shutdown safety is incomplete.** Project-browser transitions protect dirty
   data, but native window/application shutdown still needs equivalent
   unsaved-change handling.
7. **Feature-surface temptation.** UI Config files, native dialogs, terminal
   hosting, code editing, LSP integration, and hot reload are appealing, but
   none closes the current runtime/Workspace gap.

## Scope guardrails for a two-person team

### Now

- One active Project per editor window.
- One active, flat Workspace with stable entities.
- Only the transform and ASCII-renderable data needed to prove the shared path.
- Project/Workspace loading in the standalone runtime.
- A visible result: an entity authored in BasilEditor appears when Run is used.
- Tests for serialization, runtime mapping, malformed input, and generated
  Project relocation within the supported development setup.

### Next, after the shared path works

- Use the same path for a small Where Birds Nest test room.
- Complete meaningful Viewport rendering/editing and close Milestone 4.
- Add stable asset identities and focused file-change/error handling.
- Validate and refine the Project toolchain configuration story.
- Perform the deferred gameplay-feel review before stabilizing combat APIs.

### Later

- Portable JSON UI Config files.
- Built-in text/code editing and simple configurable external-editor commands.
- Platform shell integration, followed much later by a custom terminal tab.
- Dynamic game modules, safe hot reload, and in-Viewport play.
- Additive Workspaces, richer components, and hierarchy when a demonstrated
  use case requires them.
- Native file dialogs, installers/file association, and full three-platform
  release verification.

### Not now

- A general-purpose ECS or reflection framework.
- A plugin marketplace, package manager, cloud service, collaboration layer, or
  networking stack.
- A custom debugger, full IDE replacement, or terminal emulator.
- Multiple rendering backends, mobile/console support, or AAA asset pipelines.
- Broad action-RPG content systems before one editor-authored room runs through
  the reusable engine path.

## Bounded next development slice

The next slice is **Runtime Workspace Bridge**. Its acceptance result is simple:
create or edit an entity in BasilEditor, press Run, and see that entity rendered
by the generated standalone application using shared engine APIs.

The concrete schema, ownership, Text Sprite, diagnostics, migration, testing,
and implementation contract is defined in `RUNTIME_WORKSPACE_BRIDGE.md`.

The slice may introduce:

1. A C runtime API that loads a Project and its startup Workspace.
2. Minimal transform and ASCII-renderable component data with validation.
3. Mapping from persisted Workspace entities into runtime-owned state.
4. Generated entry-point code that runs that shared path.
5. Tests proving load, validation, render mapping, and the generated workflow.

The slice does not include embedded play, hot reload, hierarchy, arbitrary
components, a generalized ECS, code editing, terminal hosting, or UI Config
persistence.

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
