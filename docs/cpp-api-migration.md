# C++ embedding migration: current targets

The broad `#include <kyna/kyna.hpp>` umbrella was removed. Include and link the smallest public domain interface needed by the embedding:

- full checking/running: `kyna/language/language_session.hpp`, link `kyna_embedding`;
- token inspection: `kyna/lexing/tokenizer.hpp`, link `kyna_lexing`;
- parsing: `kyna/parsing/module_parser.hpp`, link `kyna_parsing`;
- module loading: `kyna/modules/module_loader.hpp`, link `kyna_resolution`;
- analysis: `kyna/semantics/module_analyzer.hpp`, link `kyna_typecheck`;
- execution: `kyna/execution/bytecode_virtual_machine.hpp`, link `kyna_vm`.

Use `tokenize(SourceFile)`, `parseModule(SourceFile, tokens)`, and `analyzeModuleGraph(graph)` result objects instead of catching exceptions for expected source mistakes. `LanguageSession` is the normal high-level replacement for manually sequencing the v0.1 lexer, parser, analyzer, and interpreter.

Host access is injected through `RuntimeCapabilities`. Production adapters are the default; deterministic tests can provide filesystem, process, network, and clock ports. The current build does not declare the old `kyna_lib` aggregate; use the owning target above. The public tree-walk headers remain available through `kyna_vm` during migration, but their syntax/environment contracts cannot yet be replaced by VM-backed adapters.

`LanguageSessionOptions.collectMetrics` opts into phase timing in `LanguageResult.metrics`.
Existing native adapters implementing `BytecodeNativeAdapter::invoke` continue to work.
Adapters needing language callbacks can override `invokeWithCallbacks`; its callback
interface is borrowed for that invocation only. Root managed values across re-entry.
See [the milestone inventory](bytecode-milestone.md) for remaining compatibility work.
