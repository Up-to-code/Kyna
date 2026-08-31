# Kyna VS Code support

Provides canonical `.kyna` language registration, syntax highlighting, Python-style `#` line comments, snippets, keyword and declaration completion, import-path and exported-member completion, symbols, same-file definitions, hover, CodeLens, live compiler diagnostics, Run/Check, and token/AST/HIR/MIR/bytecode inspection commands. A Run button appears in the editor title and status bar. Version 1.0.2 launches Kyna directly in the terminal, avoiding unrelated login-shell and Node-version-manager startup output.

The extension uses the purple Kyna K as its marketplace icon and matching purple light/dark `.kyna` file icons. The black-and-white icon remains under `assets/legacy/`; it is preserved for branding outside the installed extension. Run and Check toolbar icons remain theme-adaptive.

Packaged examples include first-class functions, mutable/transitive lexical closures, recursion, advanced control flow, backend filesystem/database programs, and real HTTPS clients. The closure example prints each verified result and runs through the same validated bytecode-v7 pipeline used by the CLI.

## Completion rules

- At an ordinary identifier, completion offers Kyna keywords, primitive types, standard-library names, snippets, and declarations found in the current document.
- Between the quotes of `import "..."`, completion offers `.kyna` files with paths relative to the importing file.
- After an imported alias and dot, such as `math.`, completion reads that module and offers only declarations marked `export`.
- `.`, `"`, and `/` retrigger completion automatically; normal editor completion can also be invoked manually.

Diagnostics run 250 ms after edits. Older checker processes are cancelled so a slow result cannot overwrite diagnostics for a newer buffer.

## Development install

From the repository root:

```sh
"/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code" --extensionDevelopmentPath="$PWD/editors/vscode-kyna"
```

This opens a VS Code extension-development window. To package and install permanently, run `make vscode-package`, then:

```sh
"/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code" --install-extension editors/vscode-kyna/kyna-language-support-1.0.2.vsix --force
```

Set `kyna.executable` if the CLI is not installed or available in a recognized CMake build directory. Live checking sends the unsaved buffer to the CLI together with its real file path, so relative imports resolve without writing temporary source files. The extension deliberately delegates language behavior to the CLI instead of duplicating the compiler.

## Privacy and security

The extension has no telemetry and does not send source code to an extension-owned service. Live diagnostics launch the configured local Kyna executable and pass the active unsaved buffer over stdin. Run and Check execute only after the document is saved. Kyna programs may themselves use filesystem, process, or network capabilities; review untrusted programs before running them and report vulnerabilities through the repository's private security-advisory flow.
