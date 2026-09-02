# BasilEditor Reliability Stage Closeout

**Date:** 2026-09-02  
**Alpha stage:** 2 of 9  
**Implementation status:** Complete  
**Windows interactive review:** Pending user confirmation

## Delivered behavior

### Workspace history and authoring safety

- Mutations capture bounded, owned full-document snapshots.
- Undo/redo restore validated documents, selection, and exact saved/dirty state.
- A mutation after Undo clears Redo; history is limited to 128 snapshots.
- Entity duplication assigns a stable ID and deep-copies components, including
  independently owned unknown optional JSON.
- Ctrl+Z, Ctrl+Y, and Ctrl+D expose history and duplication beside menu actions.
- Hierarchy and asset filters narrow results without changing content.

### Saving, recovery, and close protection

- Modified authoring data saves before Build or Run; Run keeps stricter asset
  preflight validation.
- About ten seconds after the latest mutation, the editor writes a separate
  `.recovery` snapshot.
- A newer snapshot offers Restore or Discard. Restore becomes unsaved work and
  never overwrites the canonical Workspace.
- Successful save removes recovery artifacts and retains `.bak` behavior.
- Explicit Discard removes recovery data.
- Project Browser return and native Windows close provide Save, Discard, and
  Cancel protection. Active child processes must be stopped first.

The Windows close guard uses a narrow platform adapter around `WM_CLOSE`, ready
for later native macOS/Linux adapters.

### Portable UI Configs

- Schema 1 is human-readable JSON containing stable panel visibility and
  editor-owned left/right/bottom layout ratios, not raw ImGui state.
- Transactional replacement retains the preceding file as `.bak`.
- Invalid files are rejected while maintained defaults remain usable.
- An explicit Project override at `.basil/editor-ui.basilui.json` takes
  precedence over the global default, which takes precedence over maintained
  defaults.
- The View menu and UI Config Manager expose save/reset operations; native
  dialogs expose import/export.

### Native Windows dialogs

- Project opening filters `.basilproject` files.
- Project creation provides a folder selector.
- UI Config import/export provides filtered file dialogs.
- Cancellation and platform errors remain distinct and dialogs do not change
  the process working directory.

## Automated verification

Focused tests cover history and dirty state, safe duplication, opaque optional
data ownership, recovery restore/discard, UI Config round trips and backups, and
invalid configuration fallback. The full runtime and editor presets remain the
stage regression gate.

## Manual Windows exit check

Open `projects/wherebirdsnest/WhereBirdsNest.basilproject` and verify:

1. Duplicate an entity, then use Ctrl+Z and Ctrl+Y.
2. Filter the Hierarchy and Assets panels.
3. Save a global UI Config, resize panels, reopen the Project, and confirm the
   saved proportions and visibility return.
4. Export and import a UI Config with native dialogs.
5. Modify an entity, wait ten seconds, terminate without saving, reopen, and
   choose Restore.
6. Modify again and press the native close button; exercise Save, Discard, and
   Cancel deliberately.

## Exit-check result

- Undo/redo and close protection: **Pass**
- Recovery without silent canonical overwrite: **Automated pass; interactive
  confirmation pending**
- UI Config and native-dialog workflows: **Build/service pass; interactive
  confirmation pending**
- Runtime regression suite: **Pass, 11/11**
- Editor-enabled regression suite: **Pass, 16/16**
