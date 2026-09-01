#include "kyna/language/language_session.hpp"
#include "../support_private.hpp"
#include "kyna/bytecode/bytecode_disassembler.hpp"
#include "kyna/bytecode/program_bytecode_compiler.hpp"
#include "kyna/hir/hir_renderer.hpp"
#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/mir/hir_lowering.hpp"
#include "kyna/mir/mir_renderer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/semantics/program_analyzer.hpp"
#include <sstream>

namespace kyna {

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
             << detail::escapeJson(token.lexeme) << "\",\"line\":" << token.location.line
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
    output << "{\"version\":1,\"module\":\"" << detail::escapeJson(name) << "\",\"declarations\":[";
  for (std::size_t index = 0; index < parsed.tree.module.declarations.size(); ++index) {
    const auto &statement = *parsed.tree.module.declarations[index];
    const auto kind = detail::statementKind(statement);
    const auto declarationName = detail::statementName(statement);
    if (json) {
      if (index)
        output << ',';
      output << "{\"kind\":\"" << kind << "\",\"name\":\"" << detail::escapeJson(declarationName)
             << "\",\"exported\":" << (detail::statementExported(statement) ? "true" : "false")
             << ",\"range\":{\"start\":{\"line\":" << statement.location.line
             << ",\"column\":" << statement.location.column
             << "},\"end\":{\"line\":" << statement.location.endLine
             << ",\"column\":" << statement.location.endColumn << "}}}";
    } else {
      output << '(' << (detail::statementExported(statement) ? "export " : "") << kind;
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
  if (detail::hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  Analyzer analyzer;
  auto semantic = analyzer.analyze(parsed.tree.module.declarations);
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (detail::hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  auto hir = lowerSyntaxToHir(name, parsed.tree, detail::standardLibraryHirOptions());
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
    return {"{\"version\":1,\"module\":\"" + detail::escapeJson(name) +
                "\",\"disassembly\":\"" + detail::escapeJson(listing) + "\"}",
            std::move(diagnostics)};
  return {listing, std::move(diagnostics)};
}

InspectionResult LanguageSession::inspectHir(std::string name, std::string source, bool json) {
  const auto sourceId = sources.add(name, std::move(source));
  auto lexed = tokenize(*sources.find(sourceId));
  auto parsed = parseModule(*sources.find(sourceId), std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (detail::hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  Analyzer analyzer;
  auto semantic = analyzer.analyze(parsed.tree.module.declarations);
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (detail::hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  auto lowered = lowerSyntaxToHir(name, parsed.tree, detail::standardLibraryHirOptions());
  diagnostics.insert(diagnostics.end(), lowered.diagnostics.begin(), lowered.diagnostics.end());
  if (!lowered.program)
    return {{}, std::move(diagnostics)};
  const auto listing = renderHir(*lowered.program);
  if (json)
    return {"{\"version\":1,\"module\":\"" + detail::escapeJson(name) +
                "\",\"hir\":\"" + detail::escapeJson(listing) + "\"}",
            std::move(diagnostics)};
  return {listing, std::move(diagnostics)};
}

InspectionResult LanguageSession::inspectMir(std::string name, std::string source, bool json) {
  const auto sourceId = sources.add(name, std::move(source));
  auto lexed = tokenize(*sources.find(sourceId));
  auto parsed = parseModule(*sources.find(sourceId), std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (detail::hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  Analyzer analyzer;
  auto semantic = analyzer.analyze(parsed.tree.module.declarations);
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (detail::hasErrors(diagnostics))
    return {{}, std::move(diagnostics)};
  auto hir = lowerSyntaxToHir(name, parsed.tree, detail::standardLibraryHirOptions());
  diagnostics.insert(diagnostics.end(), hir.diagnostics.begin(), hir.diagnostics.end());
  if (!hir.program)
    return {{}, std::move(diagnostics)};
  auto mir = lowerHirToMir(*hir.program);
  diagnostics.insert(diagnostics.end(), mir.diagnostics.begin(), mir.diagnostics.end());
  if (!mir.program)
    return {{}, std::move(diagnostics)};
  const auto listing = renderMir(*mir.program);
  if (json)
    return {"{\"version\":1,\"module\":\"" + detail::escapeJson(name) +
                "\",\"mir\":\"" + detail::escapeJson(listing) + "\"}",
            std::move(diagnostics)};
  return {listing, std::move(diagnostics)};
}

} // namespace kyna
