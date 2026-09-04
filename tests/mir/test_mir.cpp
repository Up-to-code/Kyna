#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/mir/hir_lowering.hpp"
#include "kyna/mir/mir_renderer.hpp"
#include "kyna/mir/mir_verifier.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/source/source_manager.hpp"
#include <cassert>

int main() {
  kyna::SourceManager sources;
  const auto source = sources.add("mir-test", "const left = 20; const right = 22; left + right;");
  auto lexed = kyna::tokenize(*sources.find(source));
  auto parsed = kyna::parseModule(*sources.find(source), std::move(lexed.tokens));
  auto hir = kyna::lowerSyntaxToHir("mir-test", parsed.tree);
  assert(hir.ok());
  auto mir = kyna::lowerHirToMir(*hir.program);
  assert(mir.ok());
  assert(kyna::verifyMir(*mir.program).ok());
  assert(mir.program->blocks.size() == 1);
  const auto listing = kyna::renderMir(*mir.program);
  assert(listing.find("bb0:") != std::string::npos);
  assert(listing.find("add") != std::string::npos);
  assert(listing.find("return") != std::string::npos);

  mir.program->blocks.front().terminator.reset();
  const auto invalid = kyna::verifyMir(*mir.program);
  assert(!invalid.ok());
  assert(invalid.diagnostics.front().code == "KMIR1105");

  const auto controlSource = sources.add(
      "control-flow",
      "var value = 0; while (value < 3) { value = value + 1; } if (value == 3) { return value; } else { return 0; }");
  auto controlLexed = kyna::tokenize(*sources.find(controlSource));
  auto controlParsed =
      kyna::parseModule(*sources.find(controlSource), std::move(controlLexed.tokens));
  auto controlHir = kyna::lowerSyntaxToHir("control-flow", controlParsed.tree);
  assert(controlHir.ok());
  auto controlMir = kyna::lowerHirToMir(*controlHir.program);
  assert(controlMir.ok());
  assert(controlMir.program->blocks.size() >= 7);
  const auto controlListing = kyna::renderMir(*controlMir.program);
  assert(controlListing.find("branch") != std::string::npos);
  assert(controlListing.find("goto") != std::string::npos);

  const auto functionSource = sources.add(
      "mir-functions",
      "fn add(left: int, right: int): int { return left + right; } const answer = add(20, 22);");
  auto functionLexed = kyna::tokenize(*sources.find(functionSource));
  auto functionParsed =
      kyna::parseModule(*sources.find(functionSource), std::move(functionLexed.tokens));
  auto functionHir = kyna::lowerSyntaxToHir("mir-functions", functionParsed.tree);
  assert(functionHir.ok());
  auto functionMir = kyna::lowerHirToMir(*functionHir.program);
  assert(functionMir.ok());
  assert(functionMir.program->functions.size() == 1);
  assert(functionMir.program->functions.front().parameterCount == 2);
  const auto functionListing = kyna::renderMir(*functionMir.program);
  assert(functionListing.find("mir.function @f1 add parameters=2") != std::string::npos);
  assert(functionListing.find("call @f1") != std::string::npos);

  const auto logicalSource = sources.add(
      "mir-logical", "const answer = false && (1 / 0 == 0); const fallback = true || answer;");
  auto logicalLexed = kyna::tokenize(*sources.find(logicalSource));
  auto logicalParsed =
      kyna::parseModule(*sources.find(logicalSource), std::move(logicalLexed.tokens));
  auto logicalHir = kyna::lowerSyntaxToHir("mir-logical", logicalParsed.tree);
  assert(logicalHir.ok());
  auto logicalMir = kyna::lowerHirToMir(*logicalHir.program);
  assert(logicalMir.ok());
  assert(logicalMir.program->blocks.size() >= 7);
  const auto logicalListing = kyna::renderMir(*logicalMir.program);
  assert(logicalListing.find("branch") != std::string::npos);
  assert(logicalListing.find("divide") != std::string::npos);

  const auto ifExpressionSource =
      sources.add("mir-if-expression", "const value = if (true) { 1 } else { 2 };");
  auto ifExpressionLexed = kyna::tokenize(*sources.find(ifExpressionSource));
  auto ifExpressionParsed = kyna::parseModule(*sources.find(ifExpressionSource),
                                               std::move(ifExpressionLexed.tokens));
  auto ifExpressionHir =
      kyna::lowerSyntaxToHir("mir-if-expression", ifExpressionParsed.tree);
  assert(ifExpressionHir.ok());
  auto ifExpressionMir = kyna::lowerHirToMir(*ifExpressionHir.program);
  assert(ifExpressionMir.ok());
  const auto ifExpressionListing = kyna::renderMir(*ifExpressionMir.program);
  assert(ifExpressionListing.find("branch") != std::string::npos);
  assert(ifExpressionListing.find("move") != std::string::npos);

  const auto firstClassSource = sources.add(
      "mir-first-class",
      "fn identity(value: int): int { return value; } const selected = identity; selected(1);");
  auto firstClassLexed = kyna::tokenize(*sources.find(firstClassSource));
  auto firstClassParsed =
      kyna::parseModule(*sources.find(firstClassSource), std::move(firstClassLexed.tokens));
  auto firstClassHir = kyna::lowerSyntaxToHir("mir-first-class", firstClassParsed.tree);
  assert(firstClassHir.ok());
  auto firstClassMir = kyna::lowerHirToMir(*firstClassHir.program);
  assert(firstClassMir.ok());
  const auto firstClassListing = kyna::renderMir(*firstClassMir.program);
  assert(firstClassListing.find("function @f1") != std::string::npos);
  assert(firstClassListing.find("call_indirect") != std::string::npos);

  const auto exceptionSource = sources.add(
      "mir-exceptions",
      "try { throw \"boom\"; } catch (failure) { const recovered = true; } "
      "finally { const cleaned = true; }");
  auto exceptionLexed = kyna::tokenize(*sources.find(exceptionSource));
  auto exceptionParsed =
      kyna::parseModule(*sources.find(exceptionSource), std::move(exceptionLexed.tokens));
  auto exceptionHir = kyna::lowerSyntaxToHir("mir-exceptions", exceptionParsed.tree);
  assert(exceptionHir.ok());
  auto exceptionMir = kyna::lowerHirToMir(*exceptionHir.program);
  assert(exceptionMir.ok());
  assert(exceptionMir.program->exceptionRegions.size() == 2);
  const auto exceptionListing = kyna::renderMir(*exceptionMir.program);
  assert(exceptionListing.find("throw %t") != std::string::npos);
  assert(exceptionListing.find("exception [") != std::string::npos);
}
