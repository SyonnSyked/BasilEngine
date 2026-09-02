# Runtime Workspace Bridge Technical Design

**Status:** Accepted; implementation in progress (Steps 1–6 complete)
**Design date:** 2026-09-02  
**Target slice:** BasilEditor-authored entities render in a generated standalone
Project through shared C runtime APIs

## Purpose

This document is the implementation contract for the first complete path from
BasilEditor to a running BasilEngine Project. It translates the Project Charter,
Architecture, Editor Experience Specification, and Scope Status into concrete
data, ownership, loading, rendering, and error-handling rules.

The acceptance result is intentionally narrow:

> Create or edit an entity in BasilEditor, press Run, and see that entity
> rendered by the generated standalone application from the saved Project and
> Workspace data.

This slice creates prerequisites for a genuine editor Viewport and later
play-in-editor. It does not implement either embedded gameplay or hot reload.

## Settled product behavior

- New entities are visible immediately by default.
- A visible entity can use one printable ASCII glyph or a multi-line `.txt`
  **Text Sprite**.
- Text Sprites are visual assets, not prefabs. A future prefab will describe a
  reusable entity/component composition.
- Spaces in Text Sprites are transparent by default.
- World positions are floating point and movement is not snapped to glyph cells.
- One world unit initially corresponds to one glyph cell.
- Render anchors are Bottom Center, Center, and Top Left.
- Transform2D contains position only in this revision.
- Entities without a renderable are valid.
- Empty entities receive editor-only markers and optional name labels. Those
  helpers never appear in the running game.
- Run validates and saves before launching a deterministic disk snapshot.
- Generated executables work through BasilEditor and when launched directly.
- Load failures appear in editor diagnostics and in a readable runtime error
  screen when window creation remains possible.
- Standalone runtime rendering is completed before editor Viewport rendering.

## Design principles

1. The editor and runtime call the same C loaders, validators, path resolver,
   Text Sprite decoder, component interpreter, and draw-list builder.
2. Public headers do not expose cJSON, raylib ownership, ImGui types, or parser
   memory.
3. Loading is transactional: failure does not partially replace valid state.
4. Persistent data has explicit versions, bounds, and actionable errors.
5. Unknown optional components survive load/save without being understood.
6. Unknown required components prevent use rather than producing a misleading
   partial result.
7. Project paths are relative to the Project root in persistent data.
8. The first component model is a bounded document model, not an ECS.
9. Rendering interpretation is shared, while each host owns its presentation
   target and frame lifecycle.
10. No Where Birds Nest rule or name may appear in these APIs.

## Layer and ownership model

```text
.basilproject path
        |
        v
BProjectContext ---------------- Project root and validated manifest
        |
        v
BWorkspaceDocument ------------- Heap-owned editable serialized model
        |
        +---- BTextSpriteCache -- Decoded project-relative .txt assets
        |
        v
BAsciiDrawList ----------------- Host-neutral glyph instances in world space
        |
        +---- Standalone runtime/raylib presentation
        `---- BasilEditor Viewport presentation (following step)
```

`BProjectContext` owns the normalized manifest path, Project root, loaded
`BProject`, and startup Workspace path. `BWorkspaceDocument` owns all component
records and opaque JSON payload strings. `BTextSpriteCache` owns decoded glyph
grids. A draw list contains values or stable handles, never pointers into cJSON
or temporary file buffers.

The standalone application owns these objects for its lifetime. BasilEditor’s
Workspace session owns a `BWorkspaceDocument` and replaces its current by-value
`BWorkspace` storage. The editor never mutates runtime process memory in this
slice.

## Workspace schema version 3

Schema version 3 extends each entity with a `components` array. Root identity,
stable entity IDs, `nextEntityId`, entity names, and enabled state retain their
schema-version-2 meaning.

Example:

```json
{
  "schemaVersion": 3,
  "name": "Main Workspace",
  "identifier": "Main",
  "nextEntityId": "2",
  "entities": [
    {
      "id": "entity-0000000000000001",
      "name": "Player",
      "enabled": true,
      "components": [
        {
          "type": "basil.transform2d",
          "version": 1,
          "required": true,
          "data": {
            "x": 4.0,
            "y": 7.0
          }
        },
        {
          "type": "basil.ascii-renderable",
          "version": 1,
          "required": true,
          "data": {
            "source": {
              "kind": "glyph",
              "glyph": "@"
            },
            "foreground": "#E6EDF3FF",
            "background": "#00000000",
            "layer": 0,
            "anchor": "bottom-center",
            "visible": true,
            "transparentSpaces": true
          }
        }
      ]
    }
  ]
}
```

Component type names use a lowercase namespaced identifier. Built-in types use
the `basil.` namespace. Project-defined types must use the Project identifier or
another non-`basil` namespace. Type matching is exact and case-sensitive.

Each entity may contain at most one component of a given type in this revision.
Component order is preserved for human-readable round trips but has no runtime
meaning. Every component record requires exactly `type`, `version`, `required`,
and `data`; duplicate or unknown envelope fields are rejected.

### Unknown component preservation

For a recognized component and supported version, `data` is decoded into its
typed C representation and validated strictly. For an unrecognized optional
component, `data` is serialized into a bounded, owned normalized JSON string and
retained with its envelope. Saving parses that owned payload and writes it back
as JSON. Formatting and object key order inside unknown data are not guaranteed,
but its values and structure are preserved.

An unknown component with `required: true`, or a known component with a newer
unsupported version and `required: true`, produces an unsupported-component
error. Optional unsupported components are preserved and skipped by runtime
interpretation with a warning.

This policy makes files expandable without pretending the runtime can safely
execute data it does not understand.

### Bounds

Initial safety bounds are:

- 512 entities per Workspace, retaining the current limit.
- 16 components per entity.
- 4,096 total components per Workspace.
- 4 MiB maximum Workspace file size.
- 64 KiB maximum normalized `data` payload for one unknown component.
- Existing identifier and display-name limits remain unchanged.

Limits are constants in public persistence headers and produce explicit errors.
They are prototype safety limits, not final scale promises.

## C document API

Before Step 1, `BWorkspace` was copied by value and contained fixed entity
storage. The implemented `BWorkspaceDocument` now uses explicit heap ownership
and the lifecycle API below. This prepares for opaque component payloads without
allowing casual structure assignment. The ownership contract is now active.

```c
typedef struct BWorkspaceDocument BWorkspaceDocument;

void BWorkspaceDocument_Init(BWorkspaceDocument* document);
void BWorkspaceDocument_Destroy(BWorkspaceDocument* document);

bool BWorkspaceDocument_Load(
    const char* workspace_path,
    BWorkspaceDocument* destination,
    BDiagnosticList* diagnostics
);

bool BWorkspaceDocument_Save(
    const BWorkspaceDocument* document,
    const char* workspace_path,
    BDiagnosticList* diagnostics
);

bool BWorkspaceDocument_Clone(
    const BWorkspaceDocument* source,
    BWorkspaceDocument* destination,
    BDiagnosticList* diagnostics
);
```

Load and Clone construct a temporary complete document. They replace the
destination only after parsing and validation succeed. Destroy is valid on a
zero-initialized or initialized document. Direct assignment and `memcpy` of an
owned document are forbidden.

Entity/component mutations use C functions rather than array manipulation.
Required operations for this slice are:

- add/remove entity;
- find entity by stable ID;
- add/remove/find component by type;
- get/set Transform2D position;
- get/set ASCII Renderable source, colors, layer, anchor, and visibility; and
- validate the complete document.

The editor may retain index-based presentation state, but stable IDs are the
authoritative identity. Selection must be repaired or cleared after mutations.

## Built-in component contracts

### `basil.transform2d`, version 1

```c
typedef struct BTransform2D
{
    float x;
    float y;
} BTransform2D;
```

Both values must be finite. The coordinate system is positive X to the right
and positive Y downward, matching the initial glyph canvas and raylib screen
orientation. Position is continuous; snapping is an optional editor command,
not a storage rule.

No rotation, scale, parent, velocity, or collision data is added in version 1.

### `basil.ascii-renderable`, version 1

The source is exactly one of:

```json
{ "kind": "glyph", "glyph": "@" }
```

or:

```json
{ "kind": "text-sprite", "path": "assets/sprites/player.txt" }
```

A glyph contains exactly one printable ASCII byte from `0x20` through `0x7E`.
A Text Sprite path is normalized, relative to the Project root, and cannot
escape it. Absolute paths, drive-relative paths, and traversal outside the root
are rejected.

Colors use `#RRGGBBAA`, with uppercase output and case-insensitive input. Layer
is a signed 16-bit logical draw order. Lower layers draw first; equal layers
retain entity order and then Text Sprite row/column order for deterministic
output. `visible: false` suppresses drawing without disabling the entity.

An enabled renderable requires Transform2D. An entity may omit either or both
components. A disabled entity produces no runtime draw items.

### Default new entity

The normal **Add Entity** command creates and selects:

- a generated stable ID and name;
- enabled state;
- Transform2D at `(0.0, 0.0)`; and
- ASCII Renderable using `@`, light foreground, transparent background, layer
  zero, Bottom Center anchor, visible state, and transparent spaces.

The editor also provides an explicit **Add Empty Entity** action. Removing the
renderable from an ordinary entity creates the same valid empty state.

## Text Sprite format

Text Sprites remain ordinary `.txt` files that can be created in any text
editor. Version 1 deliberately has no header or embedded metadata.

Rules:

- Bytes must be printable ASCII (`0x20`–`0x7E`) plus line endings.
- UTF-8 BOM, tabs, NUL bytes, other control bytes, and non-ASCII bytes are
  rejected with line and column information.
- LF and CRLF are accepted and normalized in memory. Lone CR is rejected.
- A final line ending does not create an extra row.
- Interior and leading spaces are retained as transparent cells.
- Trailing spaces do not increase the decoded width. This avoids semantic
  changes when editors trim line endings.
- Empty interior rows are retained. Leading and trailing empty rows therefore
  remain intentional layout.
- The file must contain at least one non-space printable glyph.
- Maximum decoded dimensions are 256 columns by 256 rows.
- Maximum source size is 128 KiB.

Rows shorter than the maximum decoded width behave as transparent cells. The
component supplies color, background, anchor, visibility, and layer; those are
not embedded in the `.txt` file.

Text Sprite errors identify the project-relative path and, where applicable,
the offending line and column. A cache entry is replaced only after a changed
file decodes successfully; later file watching may therefore retain a last
known-good asset.

## Anchor and glyph placement

For a decoded width `w` and height `h`, the anchor offset in glyph-cell units is:

| Anchor | X offset | Y offset |
| --- | ---: | ---: |
| Top Left | `0` | `0` |
| Center | `(w - 1) / 2` | `(h - 1) / 2` |
| Bottom Center | `(w - 1) / 2` | `h - 1` |

The top-left glyph position is entity position minus anchor offset. Fractional
half-cell offsets are valid for even dimensions. Glyph `(column, row)` is placed
at top-left plus `(column, row)`.

The anchor describes visual placement only. Collision, navigation, selection
bounds, and gameplay origin remain separate concerns.

## Project context and path resolution

Project discovery has this priority:

1. An explicit `--project <manifest>` command-line argument.
2. Exactly one `.basilproject` in the process working directory.
3. An upward search from the executable directory, stopping at the filesystem
   root, for a directory containing exactly one `.basilproject`.

An explicit invalid path fails immediately; it never falls through to guessing.
Zero or multiple candidates produce a readable error. BasilEditor always passes
an explicit absolute manifest path and sets the process working directory to the
Project root. Direct launch from an ordinary generated build directory therefore
finds the Project above it without compiling a machine-specific source path into
the executable.

`BProjectContext` centralizes canonical Project-root containment checks and
resolution of the startup Workspace and asset paths. Persistent paths retain
forward slashes. Platform-native separators exist only at filesystem calls.
Symlink/reparse-point containment must be checked using resolved filesystem
paths when the target exists, not lexical `..` checks alone.

## Runtime loading and rendering flow

```text
generated main
  -> discover/accept Project manifest
  -> BProjectContext_Load
  -> BWorkspaceDocument_Load(startup Workspace)
  -> BRuntimeWorkspace_Build
  -> load referenced Text Sprites transactionally
  -> create window
  -> if content is empty, show WORKSPACE ONLINE status
  -> each frame, build/sort BAsciiDrawList and present it
  -> destroy runtime Workspace, cache, document, and Project context
```

`BRuntimeWorkspace` contains runtime-owned copies/handles derived from the
document. It does not alias editable document storage. Rebuilding into a
temporary runtime Workspace and swapping on success establishes the same safety
property needed by future reload work without implementing reload now.

`BAsciiDrawList` contains deterministic glyph instances with world position,
foreground/background color, layer, and source entity ID. It is host-neutral.
The existing raylib renderer presents it in the standalone application. The
subsequent editor Viewport step consumes the same list and adds editor-only
overlays.

When no enabled renderables exist, the standalone window uses the restrained
NetRunner visual language to display:

```text
WORKSPACE ONLINE
Main.basilworkspace
0 renderable entities
```

This status is host UI, not serialized Workspace content.

## Editor behavior

### Creation and Inspector

The Add Entity control becomes a small menu or adjacent actions:

- **ASCII Character** creates the complete default entity and exposes a picker
  containing printable ASCII characters.
- **Text Sprite** creates the complete default entity and selects an existing
  Project `.txt` asset.
- **Empty Entity** creates identity, enabled state, and Transform2D at the
  origin, but no renderable. It therefore has a placeable editor marker.

The picker lists real Project files and stores only a Project-relative path. It
does not duplicate asset contents into the Workspace.

The Inspector exposes position, source kind/value, foreground/background,
layer, anchor, visible, and transparent-spaces fields. Every edit passes through
the C mutation/validation API and marks the session dirty. Invalid typed input is
reported without corrupting the last valid value.

### Empty-entity visualization

The Viewport draws a small electric-cyan crosshair at each enabled entity that
has Transform2D but no active renderable. An optional name label appears beside
it. The selected marker and label use restrained violet. Global Viewport toggles
control markers and labels; no per-entity marker style is serialized in this
slice.

An entity whose Transform2D is later removed remains valid and appears in the
Hierarchy and Inspector, but it cannot receive a spatial marker until a
Transform2D is restored.

### Save and Run

Run performs these operations in order:

1. Validate the complete in-memory Workspace.
2. Validate and decode every referenced Text Sprite.
3. Save through the existing temporary/backup replacement process.
4. Invoke the asynchronous build service.
5. Launch with `--project` and the absolute manifest path.

Any failure stops the sequence and leaves the editor active. Edits made after a
successful launch do not affect the running child. The user stops and runs again
to apply them. This rule remains until an explicit reload protocol exists.

The first implementation proves standalone rendering. The immediately following
step replaces the placeholder Workspace Viewport summary with the shared draw
list plus editor-only helpers. It does not run gameplay simulation.

## Diagnostics and failure behavior

A shared bounded diagnostic list replaces reliance on one free-form error
string for this path. Each diagnostic includes:

- severity: info, warning, or error;
- stable code;
- concise user-facing message;
- optional file path, line, and column;
- optional Workspace entity ID and component type; and
- optional technical detail for logs.

Required diagnostic families are Project discovery/path, Project parse/schema,
Workspace parse/schema, component envelope/version/data, Text Sprite IO/format,
runtime mapping, and resource exhaustion.

BasilEditor sends errors to Problems and complete detail to Console or Build
Output. Warnings do not prevent Run unless they imply missing required behavior.
The standalone runtime logs details to standard error and shows a readable error
screen if raylib initialization succeeds. It waits for explicit close or an
acknowledgement key; it must not flash and disappear. Failures before window
creation return a nonzero exit code and emit standard error.

No loader silently substitutes missing assets, clamps invalid values, downgrades
versions, or discards unknown data.

## Migration from schemas 1 and 2

- Schema 1 empty Workspaces continue to load through the existing compatibility
  path.
- Schema 2 entities load as schema 3 entities with empty component arrays.
- Compatibility loading occurs in memory and never rewrites the source file.
- The first successful save writes schema 3 through the safe-save path and
  retains the original file as `.bak`.
- A schema 2 Workspace opened in BasilEditor receives a visible migration notice
  before its first save.
- A newer unsupported root schema is rejected without modification.
- There is no downgrade writer.

Existing schema 2 entities remain empty after migration. Automatic addition of
default visual components is reserved for newly created entities; migration
must not invent content.

## Asset identity boundary

Text Sprite version 1 stores a normalized Project-relative path. This is a
deliberate bridge, not the final stable asset-identity solution from Roadmap
Milestone 3. Introducing sidecars, an asset database, import metadata, or a
rename tracker in this slice would combine two subsystems and violate the scope
checkpoint.

All Text Sprite access nevertheless goes through a small asset-reference and
cache API rather than direct `fopen` calls in components or panels. A later
schema can add a stable asset ID while retaining the path as a human-readable
location hint and migrate references explicitly.

## Verification plan

### Unit tests

- Schema 3 empty and populated round trips.
- Schema 1 and 2 compatibility without implicit writes.
- First-save backup and schema 3 migration.
- Duplicate component type, malformed envelope, unsupported version, invalid
  required/optional behavior, and unknown optional round trip.
- Transactional load/clone failure preserving the destination.
- Document lifecycle and repeated load/destroy behavior under sanitizers where
  supported.
- Finite Transform2D validation.
- ASCII Renderable glyph, color, layer, anchor, source-kind, and path validation.
- Text Sprite LF/CRLF, uneven rows, spaces, trimming, empty files, invalid bytes,
  bounds, and line/column diagnostics.
- Anchor placement for odd/even dimensions and all three anchors.
- Deterministic layer/entity/cell draw ordering.
- Project-root resolution, traversal, absolute paths, symlink/reparse escapes,
  missing files, and ambiguous discovery.
- Disabled entity and invisible renderable suppression.

### Editor/service tests

- Default visible entity and explicit empty entity creation.
- Component edits mark dirty and failed edits retain valid state.
- Unknown optional data survives an editor save.
- Run validates Text Sprites, saves before build, and passes the manifest path.
- A failed validation prevents build/launch and populates diagnostics.
- Selection remains ID-correct after entity/component mutation.

### End-to-end tests

- Generate a Project, add a glyph entity, build, launch headlessly, and verify
  the runtime mapping summary.
- Repeat with a multi-line Text Sprite and a Project path containing spaces.
- Relocate the complete generated Project and repeat without modifying source or
  manifest data.
- Direct-launch discovery succeeds from its generated build directory.
- Missing and malformed Workspace/Text Sprite failures return nonzero and emit
  stable diagnostics.

Visual presentation receives a short manual check for glyph and multi-line
placement, anchor behavior, transparency, colors, empty-Workspace status, and
the persistent error screen. Windows is the first verified host; macOS and Linux
remain unverified until their native builds and process paths are exercised.

## Implementation sequence

Each step must build, pass its focused tests, and leave a reviewable commit.

1. **Owned document foundation — Implemented:** diagnostics and lifecycle-safe,
   dynamically allocated Workspace entity storage are active without changing
   visible behavior. Load and clone replace destinations transactionally; all
   engine, generator, test, and editor callers use the single document API.
2. **Schema 3 components — Implemented:** bounded versioned envelopes,
   Transform2D and ASCII Renderable typed data, schema-1/2 migration, strict
   validation, mutation APIs, and opaque optional preservation are active.
3. **Text Sprite service — Implemented:** bounded printable-ASCII decoding,
   project-root-contained relative paths, explicit cache ownership,
   last-known-good replacement, structured diagnostics, and focused tests are
   active without raylib or editor dependencies.
4. **Shared interpretation — Implemented:** deterministic, host-neutral
   `BAsciiDrawList` snapshots now interpret Transform2D, glyph/Text Sprite ASCII
   Renderables, anchors, layers, transparency, visibility, colors, and stable
   source entity IDs through a shared C-only service.
5. **Generated runtime bridge — Implemented:** generated C, C++, and mixed
   applications use the shared runtime host to discover or accept a Project,
   load its startup Workspace and Text Sprites, render the draw list, and
   present stable empty/error states. A non-graphical validation mode supports
   relocation, direct-discovery, and failure-path integration tests.
6. **Editor authoring — Implemented:** the Hierarchy creates printable glyph,
   recursively discovered Project Text Sprite, and explicit empty entities. The
   Inspector edits identity, enabled state, position, source, colors, layer,
   anchor, visibility, and transparent spaces through transactional shared C
   mutation APIs.
7. **Run contract:** validate Text Sprites, autosave, launch with `--project`, and
   route structured diagnostics.
8. **Viewport preview:** render the same draw list with empty-entity markers and
   labels; do not host gameplay.
9. **Reference proof and audit:** author a small Where Birds Nest room through
   the same path, run the full matrix, and update the scope checkpoint.

If lifecycle conversion proves too large for one reviewable commit, step 1 may
be divided into internal storage and caller migration, but no two competing
Workspace models may remain as permanent APIs.

## Explicit exclusions

- General ECS, reflection, arbitrary native component registration, or plugins.
- Prefabs, entity hierarchy, additive Workspaces, rotation, or scale.
- Collision, navigation, physics, animation, or gameplay systems in component
  schema v1.
- Stable asset database, import pipeline, thumbnails, or file watching.
- Embedded play, simulation in the Viewport, live editing, or hot reload.
- Built-in code editor, terminal hosting, LSP, debugger, or command palette.
- Unicode/wide glyphs, rich text, embedded Text Sprite metadata, or colored
  source-file syntax.
- Packaging a content-free executable as a distributable game build.

Any newly discovered requirement that needs one of these systems pauses the
slice for a scope review rather than silently expanding it.

## Completion criteria

This design slice is complete only when:

- the technical contract has been reviewed and accepted;
- terminology agrees with the charter and editor specification;
- schema migration and unknown-data behavior are unambiguous;
- ownership and failure guarantees are testable;
- the implementation order has one visible result per bounded step; and
- no deferred subsystem is required to achieve the standalone and Viewport
  rendering proof.

The implementation slice is complete only after the editor-authored glyph and
Text Sprite examples work end to end, the documented automated tests pass, and
the recurring scope audit confirms that the next phase remains appropriate.
