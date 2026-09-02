# BasilEngine Project Charter

## Purpose

BasilEngine is a reusable application and game engine centered on ASCII and
text-art presentation. It is not merely a rendering effect for one game: the
long-term product includes a runtime, project system, editor, and project
creation workflow.

Where Birds Nest is the reference game. It exists alongside engine development
to prove that engine APIs support a real action RPG rather than hypothetical
use cases.

## Product direction

- Primary implementation language: C11
- C++ usage: only where an integration makes it necessary, initially ImGui
- Target platforms: Windows, macOS, and Linux
- Simulation: smooth, continuous, real-time movement and combat
- Presentation: glyphs and multi-glyph text art as first-class visual assets
- Editor ambition: a capable, focused tool rather than a beginner toy or a
  replacement for a full AAA toolchain
- Distribution: private personal project for now

## Reference game

Where Birds Nest is a dark-fantasy action RPG inspired structurally by Diablo,
Path of Exile, and Baldur's Gate, with a harsh setting and an ASCII-art visual
identity. Its initial role is to validate movement, combat, Workspaces, assets,
serialization, and editor workflows.

## Product principles

1. Build reusable mechanisms in BasilEngine and game rules in the project.
2. Validate important engine abstractions with a playable experiment.
3. Keep text-art assets easy to create and edit with ordinary text tools.
4. Preserve smooth simulation independently of visual glyph layout.
5. Use the same data and engine APIs in the editor and runtime.
6. Prefer understandable C designs over speculative framework machinery.
7. Design module boundaries for eventual game-code hot reload without freezing
   an ABI before gameplay requirements are understood.
8. Keep builds, paths, and serialized projects portable.
9. Add complexity only when it supports a visible runtime or tooling outcome.

## Medium-term product target

A user can launch BasilEngine, create a portable project from a template, open
it in the editor, edit and save an ASCII Workspace, write project code, build it,
and run it. Play-in-editor and safe hot reload follow once runtime and project
boundaries are stable.

## Current non-goals

- Networking or multiplayer
- Mobile and console deployment
- Multiple graphics backends
- A replacement for every general-purpose IDE or debugger capability
- A general plugin marketplace
- AAA-scale reflection or build infrastructure
- A complex ECS before reference-game needs justify it
