# Changelog

## 1.0.12

- Add a ready-to-run `GET /` homepage to new backend projects.
- Save pending editors before route generation so registry updates cannot collide with an unsaved `index.kyna`.
- Add a homepage preset and live endpoint actions for project and route items, including prompts for dynamic slug values.

## 1.0.11

- Redesign the Kyna sidebar into compact Project and Routes views instead of a list of action rows.
- Group registered routes by HTTP method with colored icons, parameter-aware tooltips, file identity, and click-to-open navigation.
- Expand the route wizard with static, `:slug`, nested multi-parameter, and custom route shapes.
- Generate handler metadata plus `request.params` and `request.query` examples, with decoded dynamic/query parameters in the HTTP runtime.

## 1.0.10

- Make `kyna.toml` the single source of truth for backend host and port in Run, Serve, and Dev.
- Add explicit Run Project and Watch & Restart CodeLens actions and move secondary manifest actions into the labeled overflow menu.
- Stop stale Run/Dev tasks before launching another backend and remove ambiguous K/R/A Explorer badges.
- Support the `ky run dev` compatibility spelling alongside canonical `ky dev`.

## 1.0.9

- Add first-class `kyna.toml` highlighting, completion, and manifest icons.
- Add a dedicated Kyna Project activity-bar view with Run, Dev, Install, Generate Route, Configure, and Open Manifest actions.
- Add matching theme-aware command icons, Explorer badges, backend snippets, HTTP application completion, and imported-member definitions.
- Support the Express-style `main → app → routes` backend architecture and `ky generate route` workflow.

## 1.0.8

- Prefer the new `ky` CLI while retaining automatic discovery of the `kyna` 1.x compatibility alias.
- Add native VS Code document formatting through `ky fmt - --source-name <file>`, including `editor.formatOnSave` support.
- Package with official `@vscode/vsce`; release packages rewrite README artwork to immutable HTTPS URLs and validate all required contents.

## 1.0.7

- Adds injected `os` and `terminal` namespace completions.
- Documents the expanded per-allocation-class heap inspector.

## 1.0.6

- Adds native TOML and XML parse/stringify completions and namespace members.
- Adds a cleanup-safe document file checkpoint snippet.

## 1.0.5

- Adds `http`, `json`, `fetchResult`, and non-throwing `http.tryFetch` completions.
- Completes inferred members for fetch responses and safe-fetch result bindings.
- Adds learning snippets for safe HTTP requests and cleanup-safe JSON files.

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
