# Open Design Questions

These questions are intentionally deferred while development focuses on the
BasilEngine project/editor workflow. They should be revisited before the combat
prototype is promoted into a stable reusable runtime model.

## Caves of Qud influence

- Which parts of Caves of Qud should influence Where Birds Nest?
- Should that influence emphasize contextual actions, bump interactions,
  look/examine mode, directional targeting, the combat log, status effects,
  keyboard-first menus, auto-actions, or another system?

## Movement and input

- Should arrow keys duplicate WASD movement?
- Should the game eventually include dodge, dash, sprint, or walk controls?
- Should mouse input support click-to-move, free aiming, target selection, or a
  combination of these?
- How should keyboard-only aiming and target selection work?
- Should targeting be required, optional soft-locking, or fully free aim?

## Combat

- Should the first developed combat model emphasize melee, ranged combat, or
  both equally?
- Should melee attacks strike one target, an arc, or every target in range?
- Can the player attack while moving?
- Should attacks have windup, recovery, movement penalties, or interruption?
- Which deeper systems are essential: equipment, resistances, penetration,
  status effects, active/passive abilities, projectiles, factions, friendly
  fire, environmental effects, or destructible terrain?

## Time and interface behavior

- Which interfaces pause simulation?
- Should inventory and dialogue pause, slow, or leave the world running?
- Should the game support active pause?
- Should complex targeting ever slow time?

## Interaction

- Should bumping into doors or objects interact automatically?
- Should item pickup be automatic, contextual, or explicitly commanded?
- Should a persistent contextual prompt identify the currently preferred
  interaction?
- How should several possible nearby interactions be ranked or cycled?

## Spatial representation

- Are ordinary actors approximately one glyph cell in size?
- Can actors and enemies use multi-character ASCII sprites?
- Do actors physically block one another?
- Is diagonal corner cutting allowed?
- How should collision relate to multi-glyph visual assets?

## Party model

- Does Where Birds Nest include companions?
- If so, are they AI-controlled, directly controlled, or switchable party
  members?

