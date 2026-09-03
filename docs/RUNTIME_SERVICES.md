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

The subsequent Stage 6 slices expose this service through the versioned module
host table and add safe Workspace replacement, collision, ASCII UI/dialogue,
and bounded sound/music playback.
