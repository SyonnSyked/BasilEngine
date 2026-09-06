# Alpha Runtime Services

Stage 6 turns the native game-module boundary into a practical gameplay API.
The implementation order is intentionally dependency-driven: input and safe
Workspace replacement, collision-facing queries, ASCII UI, then audio. These
remain bounded services rather than a general ECS, physics engine, UI toolkit,
or audio-authoring environment.

## Input foundation

Named actions are Project-owned in `.basil/input.json` schema version 1. Each
entry has a stable name, a `keyboard` or `mouse` device, and a platform input
code. Reads are limited to 64 KiB and 128 actions. Loading is transactional;
malformed, oversized, unsupported, or duplicate data leaves the previous valid
map active. The C API supports registration, keyboard/mouse rebinding,
pressed/down/released state, binding inspection, and explicit focus suppression.

Generated Projects receive movement, confirm/cancel, and primary mouse-action
defaults as ordinary editable JSON. The runtime suppresses gameplay actions
while its window lacks focus. Gamepads remain deferred by the alpha contract.

The versioned module host table now exposes named input, Collider2D bounds and
queries, and safe transactional replacement of the single active Workspace.
Replacement is deferred until after game callbacks; failure retains the active
Workspace, and successful replacement advances the generation used to reject
stale entity handles.

## Screen-space ASCII UI

Project code uses the `BGame.h` boundary to begin a transient UI pass, draw
labels and filled ASCII-border boxes, submit ordered choices, and end the pass.
Nine screen anchors use cell offsets and visible-grid clipping. The runtime
draws these commands after the world draw list with a top-left screen-space
origin, so camera/world movement cannot move HUD or dialogue content.

Choice selection wraps through the existing `move_up` and `move_down` actions;
`confirm` activates the selection. The existing `primary_action` mouse binding
supports cell hover, selection, and click activation. `BGameUI_End` reports
whether navigation or activation input was handled, allowing game code to
avoid applying the same action to gameplay. Choice meaning and dialogue state
remain owned by the game. This is an immediate-mode game overlay, not an editor
UI or retained GUI framework.

Bounded sound/music playback remains a later Stage 6 slice.
