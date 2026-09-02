# Integrated Programming Workflow

Stage 5 makes BasilEditor sufficient for the ordinary edit, build, diagnose,
run, and terminal loop while preserving external-editor workflows.

The Code Editor uses the shared Project-contained document service. It provides
a filtered Project tree, multiple tabs, create/rename/delete/reload, transactional
save and Save All, dirty close protection, external-change/conflict detection,
find/replace, go-to-line, line numbers, language-aware syntax inspection,
indentation helpers, and matching-bracket navigation. Build Problems open the
matching Project file and position the cursor at the compiler line.

External editor and shell executables are saved in global schema-v2 editor
preferences through `Tools > Programming Tools`. `BASIL_EXTERNAL_EDITOR` and
`BASIL_TERMINAL` remain deliberate per-process overrides. The internal editor is
the default; external tools operate on the same ordinary files.

The Terminal panel owns one persistent Project-root shell process. It captures
bounded combined stdout/stderr, accepts commands, supports copy/selection through
normal ImGui text behavior, and provides clear and restart. Closing or switching
the Project terminates the child process. Windows PowerShell is the alpha default.

## Stage 5 Windows exit check

1. Create or open a generated Project and open `source/game.c` or `game.cpp` in
   Code Editor.
2. Make a small valid edit, Save All, Build, Run, Stop, edit again, and rerun.
3. Introduce a compiler error. Build, select the Problems entry, and confirm the
   correct file and line opens. Repair it and build successfully.
4. Make an unsaved edit and verify tab/Project close refuses silent loss. Modify
   the same file externally and verify the conflict is reported rather than
   overwritten; Reload must resolve it explicitly.
5. Configure or retain the external editor, open the active file externally,
   save a change, and confirm BasilEditor refreshes it.
6. Open Terminal, run `Get-Location` and `Write-Output BASIL_TERMINAL_OK`, confirm
   the Project root and marker appear, then verify Clear and Restart.

Automated coverage verifies document containment and lifecycle, preference
migration/persistence, real terminal stdin/stdout/restart/stop, generated Project
builds, and all prior engine/editor behavior.
