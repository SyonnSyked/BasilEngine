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

For ordinary solid-blocking movement, Project code calls
`BGame_MoveWithCollision(host, entity, deltaX, deltaY)` on an enabled entity with
Transform2D and Collider2D. Resolution is deterministic and axis-separated (X,
then Y); trigger and disabled colliders do not block, and Collider2D remains
attached to Transform2D rather than owning a separate position.
`BGame_IsTriggerOverlapping` directly checks a requested trigger entity.
`BGame_QueryColliders` remains available for games that need custom collision
behavior. These conveniences are kinematic AABB services, not a rigid-body
physics system.

## Screen-space ASCII UI

Project code uses the `BGame.h` boundary to begin a transient UI pass, draw
labels and filled ASCII-border boxes, submit ordered choices, and end the pass.
Nine screen anchors use cell offsets and visible-grid clipping. The runtime
draws these commands after the world draw list with a top-left screen-space
origin, so camera/world movement cannot move HUD or dialogue content.

The game queries its own named actions and passes previous, next, confirm, and
pointer-activation intent in `BGameUIInput`; Basil does not impose action names.
The runtime owns physical pointer-to-cell mapping. `BGameUI_End` reports input
handled during UI construction only; it cannot retroactively consume input
already processed during Update. Modal gameplay such as WBN dialogue therefore
suppresses movement explicitly in game-owned state.

Anchored boxes return their resolved screen-cell origin for panel-local child
offsets. Box generation clips before iteration, and ordinary full-screen panels
fit the bounded command buffer. Where Birds Nest combines Collider2D room
walls, a Seamus trigger, modal HUD/dialogue, dialogue-requested Workspace
replacement, generation detection, handle reacquisition, and continued movement
in a visible second room. Audio is the remaining Stage 6 runtime-service slice.
