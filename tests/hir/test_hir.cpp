#include "kyna/hir/hir_renderer.hpp"
#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/source/source_manager.hpp"
#include <cassert>

int main() {
  kyna::SourceManager sources;
  const auto source = sources.add("hir-test", "set left = 20; set right = 22; left + right;");
  auto lexed = kyna::tokenize(*sources.find(source));
  auto parsed = kyna::parseModule(*sources.find(source), std::move(lexed.tokens));
  assert(lexed.diagnostics.empty());
  assert(parsed.diagnostics.empty());

  auto lowered = kyna::lowerSyntaxToHir("hir-test", parsed.tree);
  assert(lowered.ok());
  assert(lowered.program->locals.size() == 2);
  assert(lowered.program->body.size() == 3);
  const auto listing = kyna::renderHir(*lowered.program);
  assert(listing.find("local %l0 left immutable") != std::string::npos);
  assert(listing.find("add") != std::string::npos);

  const auto functionSource = sources.add(
      "hir-functions",
      "func add(left: int, right: int): int { return left + right; } set answer = add(20, 22);");
  auto functionLexed = kyna::tokenize(*sources.find(functionSource));
  auto functionParsed =
      kyna::parseModule(*sources.find(functionSource), std::move(functionLexed.tokens));
  auto functionHir = kyna::lowerSyntaxToHir("hir-functions", functionParsed.tree);
  assert(functionHir.ok());
  assert(functionHir.program->functions.size() == 1);
  assert(functionHir.program->functions.front().parameters.size() == 2);
  const auto functionListing = kyna::renderHir(*functionHir.program);
  assert(functionListing.find("function @f0 add") != std::string::npos);
  assert(functionListing.find("call @f0") != std::string::npos);

  const auto logicalSource = sources.add("hir-logical", "set value = true && false || true;");
  auto logicalLexed = kyna::tokenize(*sources.find(logicalSource));
  auto logicalParsed =
      kyna::parseModule(*sources.find(logicalSource), std::move(logicalLexed.tokens));
  auto logicalHir = kyna::lowerSyntaxToHir("hir-logical", logicalParsed.tree);
  assert(logicalHir.ok());
  const auto logicalListing = kyna::renderHir(*logicalHir.program);
  assert(logicalListing.find("and") != std::string::npos);
  assert(logicalListing.find("or") != std::string::npos);

  const auto ifExpressionSource =
      sources.add("hir-if-expression", "set value = if (true) { 1 } else { 2 };");
  auto ifExpressionLexed = kyna::tokenize(*sources.find(ifExpressionSource));
  auto ifExpressionParsed = kyna::parseModule(*sources.find(ifExpressionSource),
                                               std::move(ifExpressionLexed.tokens));
  auto ifExpressionHir =
      kyna::lowerSyntaxToHir("hir-if-expression", ifExpressionParsed.tree);
  assert(ifExpressionHir.ok());
  assert(kyna::renderHir(*ifExpressionHir.program).find(" then ") != std::string::npos);

  const auto firstClassSource = sources.add(
      "hir-first-class",
      "func identity(value: int): int { return value; } set selected = identity; selected(1);");
  auto firstClassLexed = kyna::tokenize(*sources.find(firstClassSource));
  auto firstClassParsed =
      kyna::parseModule(*sources.find(firstClassSource), std::move(firstClassLexed.tokens));
  auto firstClassHir = kyna::lowerSyntaxToHir("hir-first-class", firstClassParsed.tree);
  assert(firstClassHir.ok());
  const auto firstClassListing = kyna::renderHir(*firstClassHir.program);
  assert(firstClassListing.find("function @f0") != std::string::npos);
  assert(firstClassListing.find("call.indirect") != std::string::npos);

  const auto nativeMemberSource = sources.add(
      "hir-native-members",
      "set metadata = process.json(\"{\\\"ready\\\":true}\"); console.log(metadata.ready);");
  auto nativeMemberLexed = kyna::tokenize(*sources.find(nativeMemberSource));
  auto nativeMemberParsed =
      kyna::parseModule(*sources.find(nativeMemberSource), std::move(nativeMemberLexed.tokens));
  auto nativeMemberHir = kyna::lowerSyntaxToHir(
      "hir-native-members", nativeMemberParsed.tree,
      kyna::HirLoweringOptions{{}, {{"process.json", "jsonParse"}, {"console.log", "log"}}});
  assert(nativeMemberHir.ok());
  const auto nativeMemberListing = kyna::renderHir(*nativeMemberHir.program);
  assert(nativeMemberListing.find("call.native jsonParse") != std::string::npos);
  assert(nativeMemberListing.find("call.native log") != std::string::npos);

  const auto exceptionSource = sources.add(
      "hir-exceptions",
      "try { throw \"boom\"; } catch (failure) { set recovered = true; } "
      "finally { set cleaned = true; }");
  auto exceptionLexed = kyna::tokenize(*sources.find(exceptionSource));
  auto exceptionParsed =
      kyna::parseModule(*sources.find(exceptionSource), std::move(exceptionLexed.tokens));
  auto exceptionHir = kyna::lowerSyntaxToHir("hir-exceptions", exceptionParsed.tree);
  assert(exceptionHir.ok());
  const auto exceptionListing = kyna::renderHir(*exceptionHir.program);
  assert(exceptionListing.find("throw") != std::string::npos);
  assert(exceptionListing.find(" catch ") != std::string::npos);
  assert(exceptionListing.find(" finally ") != std::string::npos);

  const auto classSource = sources.add(
      "hir-classes",
      "class Animal { public name: str; public init(name: str) { self.name = name; } "
      "public func speak(): str { return self.name; } } "
      "class Dog extends Animal { public override func speak(): str { return self.name; } } "
      "set pet = new Dog(\"Rex\"); pet.speak();");
  auto classLexed = kyna::tokenize(*sources.find(classSource));
  auto classParsed =
      kyna::parseModule(*sources.find(classSource), std::move(classLexed.tokens));
  auto classHir = kyna::lowerSyntaxToHir("hir-classes", classParsed.tree);
  assert(classHir.ok());
  assert(classHir.program->classes.size() == 2);
  assert(classHir.program->classes[1].parent == kyna::HirClassId{0});
  assert(classHir.program->classes[0].constructor.has_value());
  assert(classHir.program->functions.size() == 3);
  assert(classHir.program->functions[0].parameters.size() == 2);
  const auto classListing = kyna::renderHir(*classHir.program);
  assert(classListing.find("class @c1 Dog extends @c0") != std::string::npos);
  assert(classListing.find("new @c1") != std::string::npos);
  assert(classListing.find("assign.member") != std::string::npos);
}
