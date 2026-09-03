# BasilEngine Alpha Asset Foundation Closeout

**Date:** 2026-09-02  
**Alpha stage:** 3 of 9  
**Implementation status:** Complete  
**Windows interactive review:** Pending user confirmation

## Stable identity model

Each opened Project owns a versioned `.basil/assets.json` registry. Every
supported asset receives a stable ID while retaining its normalized,
Project-relative path as the readable location hint.

The registry recognizes `.txt` Text Sprites, `.json` data, `.ttf`/`.otf` fonts,
and `.wav`/`.ogg` audio. Initial identity is deterministic within the Project.
Scans preserve identity by path for edits and by a unique content fingerprint
for external moves. Duplicate records, malformed JSON, oversized assets, and
resolved paths outside `assets/` are rejected.

Workspace schema 3 continues to store readable Text Sprite paths. The registry
is the identity authority, and moves transactionally remap matching Workspace
references through history-aware mutation APIs. This avoids a premature schema
revision while establishing identity for later metadata and export work.

## Change detection and failure behavior

- BasilEditor scans once per second while a Project is open.
- Size, fingerprint, and modification time distinguish changes.
- Internal and external create, edit, move, and delete operations update the
  catalog.
- Changes invalidate the Viewport Text Sprite cache and draw list; malformed
  data cannot replace its last valid preview.
- Reference remapping participates in undo/redo and dirty-state protection.
- Registry writes use temporary replacement, retain `.bak`, and restore the
  prior file when possible if replacement fails.
- Individual catalogued assets are bounded to 256 MiB.

Fonts and audio are catalogued now; runtime decoding/playback belongs to the
later playable-runtime-services stage.

## Text Sprite editor

The dockable editor provides bounded multiline ASCII editing, transactional
last-known-good preview, dimensions, electric-cyan color preview, transparent
space dots, safe replacement with backup, explicit external-change resolution,
native creation inside `assets/`, Save All, and close protection.

Tabs, control/non-ASCII bytes, empty visual content, excessive dimensions,
escaped paths, and resolved symlink/reparse escapes are rejected through shared
engine rules. Rectangular selection, stamping, animation, and rich painting
tools remain deferred.

## Automated verification

- Stable IDs survive edits, editor rename, external move, and service restart.
- Registry creation and all alpha asset classifications are verified.
- Text Sprite preview/save/backup/failure, external reload, creation, and path
  rejection are verified.
- Workspace remapping and Undo are verified.
- UI Config schema 1 migrates to schema 2 with the new panel state.
- Runtime regression suite: **11/11 passed**
- Editor-enabled regression suite: **18/18 passed**
- Where Birds Nest Project startup/native-close smoke test: **passed**

## Manual Windows exit check

Open `projects/wherebirdsnest/WhereBirdsNest.basilproject`:

1. Select `assets/player.txt` and confirm the Text Sprite Editor opens.
2. Edit it and verify dimensions, transparent-space dots, and preview update.
3. Enter a tab and verify an error appears while the prior preview remains.
4. Correct and save it, then verify the Workspace Viewport refreshes.
5. Right-click and rename it; verify its Workspace reference updates and Undo
   restores the prior path.
6. Change it in Neovim and confirm external-change controls appear.
7. Create a new Text Sprite and reopen the Project to confirm identity persists.

Implementation is closed; interaction remains pending until operator review.
