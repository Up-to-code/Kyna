# Changelog

## 1.0.4

- Render compiler errors and warnings with ANSI color in the managed Run/Check terminal.
- Keep inspection panels and machine-readable live diagnostics free of ANSI escape sequences.

## 1.0.3

- Capture CLI stdout/stderr through VS Code `ProcessExecution` without invoking the user's shell.
- Dispose stale shell-backed Run/Check terminals before every launch.
- Prefer the CLI produced by the selected CMake preset over legacy build directories.
- Stop with an explicit editor error when a modified source file cannot be saved.

## 1.0.2

- Launch Run and Check as direct Kyna terminal processes so login-shell and Node manager warnings do not obscure program output.
- Make the closure example print its calculated results.

## 1.0.1

- Adds document symbols, same-file definitions, hover details, and Run/Check CodeLens.
- Adds token, syntax-tree, HIR, MIR, and bytecode inspection commands backed by the Kyna CLI.
- Adds Unicode text standard-library completions and packages the latest backend examples.
- Ensures packaging replaces stale local VSIX artifacts instead of accumulating old versions.

## 1.0.0

- Registers only `.kyna` source files.
- Uses the purple Kyna icon for the marketplace and file experience.
- Adds `#` comments, snippets, declarations, import/export completion, live `kyna.diagnostic/v1` diagnostics, and Run/Check actions.
- Detects preset and conventional build-directory CLI locations.
- Preserves black-and-white artwork under `assets/legacy/` without using it as the installed icon.
- Packages the bytecode-v4 lexical-closure example with mutable, transitive, and recursive captures.
