#include "kyna/language/language_session.hpp"
#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/bytecode/bytecode_disassembler.hpp"
#include "kyna/bytecode/program_bytecode_compiler.hpp"
#include "kyna/execution/bytecode_virtual_machine.hpp"
#include "kyna/hir/hir_renderer.hpp"
#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/mir/hir_lowering.hpp"
#include "kyna/mir/mir_renderer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"
#include "kyna/stdlib/bytecode_standard_library.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace kyna {
namespace {
bool hasErrors(const std::vector<Diagnostic> &diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic &diagnostic) { return !diagnostic.warning; });
}

bool requiresHostServer(const std::filesystem::path &entry) {
  std::ifstream file(entry, std::ios::binary);
  if (!file) return false;
  std::ostringstream contents; contents << file.rdbuf();
  return contents.str().find("http.server") != std::string::npos;
}

struct BytecodeAttempt {
  bool supported{false};
  std::vector<Diagnostic> diagnostics;
  HeapStats heapStats;
};

HirLoweringOptions standardLibraryHirOptions() {
  return {
      bytecodeStandardLibraryFunctionNames(),
      {{"console.log", "log"},
       {"process.json", "jsonParse"},
       {"process.stringify", "jsonStringify"},
       {"process.run", "processRun"},
       {"process.env", "processEnv"},
       {"os.name", "osName"},
       {"os.architecture", "osArchitecture"},
       {"os.cwd", "osWorkingDirectory"},
       {"terminal.interactive", "terminalIsInteractive"},
       {"terminal.supportsColor", "terminalSupportsColor"},
       {"http.fetch", "fetch"},
       {"http.tryFetch", "fetchResult"},
       {"json.parse", "jsonParse"},
       {"json.stringify", "jsonStringify"},
       {"toml.parse", "tomlParse"},
       {"toml.stringify", "tomlStringify"},
       {"xml.parse", "xmlParse"},
       {"xml.stringify", "xmlStringify"},
       {"fs.read", "readFile"},
       {"fs.write", "writeFile"},
       {"fs.readJson", "readJsonFile"},
       {"fs.writeJson", "writeJsonFile"},
       {"fs.createDirectory", "createDirectory"},
       {"fs.exists", "fileExists"},
       {"fs.remove", "removePath"},
       {"fs.list", "listDirectory"},
       {"collections.unique", "unique"},
       {"collections.sort", "sort"}}};
}

BytecodeAttempt executeBytecodeSubset(const std::string &name, const SyntaxTree &tree,
                                      RuntimeCapabilities capabilities) {
  auto hir = lowerSyntaxToHir(name, tree, standardLibraryHirOptions());
  if (!hir.program) {
    const bool onlyUnsupported =
        !hir.diagnostics.empty() &&
        std::all_of(hir.diagnostics.begin(), hir.diagnostics.end(),
                    [](const Diagnostic &diagnostic) { return diagnostic.code == "KHIR1201"; });
    return {!onlyUnsupported, onlyUnsupported ? std::vector<Diagnostic>{}
                                             : std::move(hir.diagnostics), {}};
  }
  auto mir = lowerHirToMir(*hir.program);
  if (!mir.program)
    return {true, std::move(mir.diagnostics), {}};
  auto bytecode = compileMirToBytecode(*mir.program);
  if (!bytecode.module)
    return {true, std::move(bytecode.diagnostics), {}};
  auto nativeLibrary = createBytecodeStandardLibrary(std::move(capabilities), std::cout);
  auto execution = BytecodeVirtualMachine().execute(*bytecode.module, nativeLibrary.get());
  return {true, std::move(execution.diagnostics), execution.heapStats};
}

std::string escapeJson(std::string_view value) {
  std::ostringstream output;
  for (const char character : value) {
    switch (character) {
    case '\\':
      output << "\\\\";
      break;
    case '"':
      output << "\\\"";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      output << character;
      break;
    }
  }
  return output.str();
}

std::string statementKind(const Stmt &statement) {
  return std::visit(
      [](const auto &node) -> std::string {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ImportDecl>)
          return "import";
        if constexpr (std::is_same_v<T, VarDecl>)
          return node.mutableBinding ? "let" : "set";
        if constexpr (std::is_same_v<T, FunctionDecl>)
          return "function";
        if constexpr (std::is_same_v<T, ClassDecl>)
          return "class";
        if constexpr (std::is_same_v<T, InterfaceDecl>)
          return "interface";
        if constexpr (std::is_same_v<T, BlockStmt>)
          return "block";
        if constexpr (std::is_same_v<T, IfStmt>)
          return "if";
        if constexpr (std::is_same_v<T, WhileStmt>)
          return "while";
        if constexpr (std::is_same_v<T, LoopStmt>)
          return "loop";
        if constexpr (std::is_same_v<T, ReturnStmt>)
          return "return";
        if constexpr (std::is_same_v<T, ThrowStmt>)
          return "throw";
        if constexpr (std::is_same_v<T, TryStmt>)
          return "try";
        if constexpr (std::is_same_v<T, ExprStmt>)
          return "expression";
        return "statement";
      },
      statement.node);
}

std::string statementName(const Stmt &statement) {
  return std::visit(
      [](const auto &node) -> std::string {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ImportDecl>)
          return node.alias;
        if constexpr (std::is_same_v<T, VarDecl> || std::is_same_v<T, FunctionDecl> ||
                      std::is_same_v<T, ClassDecl> || std::is_same_v<T, InterfaceDecl>)
          return node.name;
        return {};
      },
      statement.node);
}

bool statementExported(const Stmt &statement) {
  return std::visit(
      [](const auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, VarDecl> || std::is_same_v<T, FunctionDecl> ||
                      std::is_same_v<T, ClassDecl> || std::is_same_v<T, InterfaceDecl>)
          return node.exported;
        return false;
      },
      statement.node);
}
} // namespace

bool LanguageResult::ok() const { return !hasErrors(diagnostics); }
bool InspectionResult::ok() const { return !hasErrors(diagnostics); }

LanguageSession::LanguageSession(LanguageSessionOptions sessionOptions)
    : options(std::move(sessionOptions)), executor(options.capabilities, installStandardLibrary) {
  interactiveAnalyzer.setInteractive(true);
}

AnalysisResult LanguageSession::compile(const std::filesystem::path &entry,
                                        std::vector<Diagnostic> &frontEnd) {
  auto loaded = loadModuleGraph(sources, entry, ModuleLoadOptions{options.modulePaths});
  frontEnd = loaded.diagnostics;
  if (!loaded.ok())
    return {std::nullopt, {}};
  return analyzeModuleGraph(std::move(loaded.graph));
}

LanguageResult LanguageSession::check(const std::filesystem::path &entry) {
  std::vector<Diagnostic> diagnostics;
  auto analysis = compile(entry, diagnostics);
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  return {std::move(diagnostics), false, {}};
}

LanguageResult LanguageSession::run(const std::filesystem::path &entry) {
  std::vector<Diagnostic> diagnostics;
  auto analysis = compile(entry, diagnostics);
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  if (!analysis.program || hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}};
  if (!requiresHostServer(entry) && analysis.program->modules.modules.size() == 1) {
    const auto module = analysis.program->modules.modules.find(analysis.program->modules.entry);
    if (module != analysis.program->modules.modules.end() && module->second.dependencies.empty()) {
      auto attempt = executeBytecodeSubset(entry.string(), module->second.syntax,
                                           options.capabilities);
      if (attempt.supported) {
        diagnostics.insert(diagnostics.end(), attempt.diagnostics.begin(), attempt.diagnostics.end());
        const bool executed = !hasErrors(diagnostics);
        return {std::move(diagnostics), executed, attempt.heapStats};
      }
    }
  }
  auto execution = executor.execute(*analysis.program);
  diagnostics.insert(diagnostics.end(), execution.diagnostics.begin(), execution.diagnostics.end());
  return {std::move(diagnostics), execution.ok(), executor.runtime().heap().stats()};
}

LanguageResult LanguageSession::checkSource(std::string name, std::string source) {
  const auto sourceId = sources.add(name, std::move(source));
  const auto &file = *sources.find(sourceId);
  auto lexed = tokenize(file);
  auto parsed = parseModule(file, std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}};
  ParsedModuleGraph graph;
  graph.entry = name;
  graph.initializationOrder.push_back(name);
  graph.modules.emplace(name, ModuleRecord{std::move(parsed.tree), {}});
  auto analysis = analyzeModuleGraph(std::move(graph));
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  return {std::move(diagnostics), false, {}};
}

LanguageResult LanguageSession::checkSourceAtPath(const std::filesystem::path &entry,
                                                  std::string source) {
  auto loaded = loadModuleGraphWithEntrySource(sources, entry, std::move(source),
                                               ModuleLoadOptions{options.modulePaths});
  auto diagnostics = std::move(loaded.diagnostics);
  if (!loaded.ok())
    return {std::move(diagnostics), false, {}};
  auto analysis = analyzeModuleGraph(std::move(loaded.graph));
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  return {std::move(diagnostics), false, {}};
}

LanguageResult LanguageSession::runSource(std::string name, std::string source, bool interactive) {
  const auto sourceId = sources.add(name, std::move(source));
  const auto &file = *sources.find(sourceId);
  auto lexed = tokenize(file);
  auto parsed = parseModule(file, std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}};
  Analyzer analyzer;
  auto semantic = interactive ? interactiveAnalyzer.analyze(parsed.tree.module.declarations)
                              : analyzer.analyze(parsed.tree.module.declarations);
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}};
  // Interactive submissions must share one runtime environment. The bytecode
  // fast path creates a fresh VM for each source unit, which is correct for a
  // standalone program but would discard declarations and values in a REPL.
  if (!interactive) {
    auto attempt = executeBytecodeSubset(name, parsed.tree, options.capabilities);
    if (attempt.supported) {
      diagnostics.insert(diagnostics.end(), attempt.diagnostics.begin(), attempt.diagnostics.end());
      const bool executed = !hasErrors(diagnostics);
      return {std::move(diagnostics), executed, attempt.heapStats};
    }
  }
  try {
    executor.runtime().execute(parsed.tree.module.declarations);
    return {std::move(diagnostics), true, executor.runtime().heap().stats()};
  } catch (const KynaError &error) {
    diagnostics.push_back(error.diagnostic);
    return {std::move(diagnostics), false, executor.runtime().heap().stats()};
  }
}

InspectionResult LanguageSession::inspectTokens(std::string name, std::string source, bool json) {
  const auto sourceId = sources.add(name, std::move(source));
  auto lexed = tokenize(*sources.find(sourceId));
  std::ostringstream output;
  if (json)
    output << "{\"version\":1,\"tokens\":[";
  for (std::size_t index = 0; index < lexed.tokens.size(); ++index) {
    const auto &token = lexed.tokens[index];
    if (json) {
      if (index)
        output << ',';
      output << "{\"kind\":\"" << tokenName(token.kind) << "\",\"lexeme\":\""
             << escapeJson(token.lexeme) << "\",\"line\":" << token.location.line
             << ",\"column\":" << token.location.column << '}';
    } else {
      output << token.location.line << ':' << token.location.column << ' ' << tokenName(token.kind)
             << "  " << token.lexeme << '\n';
    }
  }
  if (json)
    output << "]}";
  return {output.str(), std::move(lexed.diagnostics)};
}

InspectionResult LanguageSession::inspectSyntax(std::string name, std::string source, bool json) {
  const auto sourceId = sources.add(name, std::move(source));
  auto lexed = tokenize(*sources.find(sourceId));
  auto parsed = parseModule(*sources.find(sourceId), std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  std::ostringstream output;
  if (json)
    output << "{\"version\":1,\"module\":\"" << escapeJson(name) << "\",\"declarations\":[";
  for (std::size_t index = 0; index < parsed.tree.module.declarations.size(); ++index) {
    const auto &statement = *parsed.tree.module.declarations[index];
    const auto kind = statementKind(statement);
    const auto declarationName = statementName(statement);
    if (json) {
      if (index)
        output << ',';
      output << "{\"kind\":\"" << kind << "\",\"name\":\"" << escapeJson(declarationName)
             << "\",\"exported\":" << (statementExported(statement) ? "true" : "false")
             << ",\"range\":{\"start\":{\"line\":" << statement.location.line
             << ",\"column\":" << statement.location.column
             << "},\"end\":{\"line\":" << statement.location.endLine
             << ",\"column\":" << statement.location.endColumn << "}}}";
    } else {
      output << '(' << (statementExported(statement) ? "export " : "") << kind;
      if (!declarationName.empty())
        output << ' ' << declarationName;
      output << " @" << statement.location.line << ':' << statement.location.column << ")\n";
    }
  }
  if (json)
    output << "]}";
  return {output.str(), std::move(diagnostics)};
}

InspectionResult LanguageSession::inspectBytecode(std::string name, std::string source, bool json) {
  const auto sourceId = sources.add(name, std::move(source));
  auto lexed = tokenize(*sources.find(sourceId));
  auto parsed = parseModule(*sources.find(sourceId), std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  Analyzer analyzer;
  auto semantic = analyzer.analyze(parsed.tree.module.declarations);
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  auto hir = lowerSyntaxToHir(name, parsed.tree, standardLibraryHirOptions());
  diagnostics.insert(diagnostics.end(), hir.diagnostics.begin(), hir.diagnostics.end());
  if (!hir.program)
    return {{}, std::move(diagnostics)};
  auto mir = lowerHirToMir(*hir.program);
  diagnostics.insert(diagnostics.end(), mir.diagnostics.begin(), mir.diagnostics.end());
  if (!mir.program)
    return {{}, std::move(diagnostics)};
  auto compiled = compileMirToBytecode(*mir.program);
  diagnostics.insert(diagnostics.end(), compiled.diagnostics.begin(), compiled.diagnostics.end());
  if (!compiled.module)
    return {{}, std::move(diagnostics)};
  const auto listing = disassembleBytecode(*compiled.module);
  if (json)
    return {"{\"version\":1,\"module\":\"" + escapeJson(name) +
                "\",\"disassembly\":\"" + escapeJson(listing) + "\"}",
            std::move(diagnostics)};
  return {listing, std::move(diagnostics)};
}

InspectionResult LanguageSession::inspectHir(std::string name, std::string source, bool json) {
  const auto sourceId = sources.add(name, std::move(source));
  auto lexed = tokenize(*sources.find(sourceId));
  auto parsed = parseModule(*sources.find(sourceId), std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  Analyzer analyzer;
  auto semantic = analyzer.analyze(parsed.tree.module.declarations);
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  auto lowered = lowerSyntaxToHir(name, parsed.tree, standardLibraryHirOptions());
  diagnostics.insert(diagnostics.end(), lowered.diagnostics.begin(), lowered.diagnostics.end());
  if (!lowered.program)
    return {{}, std::move(diagnostics)};
  const auto listing = renderHir(*lowered.program);
  if (json)
    return {"{\"version\":1,\"module\":\"" + escapeJson(name) +
                "\",\"hir\":\"" + escapeJson(listing) + "\"}",
            std::move(diagnostics)};
  return {listing, std::move(diagnostics)};
}

InspectionResult LanguageSession::inspectMir(std::string name, std::string source, bool json) {
  const auto sourceId = sources.add(name, std::move(source));
  auto lexed = tokenize(*sources.find(sourceId));
  auto parsed = parseModule(*sources.find(sourceId), std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  Analyzer analyzer;
  auto semantic = analyzer.analyze(parsed.tree.module.declarations);
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  auto hir = lowerSyntaxToHir(name, parsed.tree, standardLibraryHirOptions());
  diagnostics.insert(diagnostics.end(), hir.diagnostics.begin(), hir.diagnostics.end());
  if (!hir.program)
    return {{}, std::move(diagnostics)};
  auto mir = lowerHirToMir(*hir.program);
  diagnostics.insert(diagnostics.end(), mir.diagnostics.begin(), mir.diagnostics.end());
  if (!mir.program)
    return {{}, std::move(diagnostics)};
  const auto listing = renderMir(*mir.program);
  if (json)
    return {"{\"version\":1,\"module\":\"" + escapeJson(name) +
                "\",\"mir\":\"" + escapeJson(listing) + "\"}",
            std::move(diagnostics)};
  return {listing, std::move(diagnostics)};
}

} // namespace kyna
