#include "kyna/parsing/recursive_descent_parser.hpp"
#include "kyna/semantics/program_validation.hpp"
#include <algorithm>
#include <cassert>

int main() {
  auto valid = kyna::Parser(kyna::lex("intf Printable { name: str; } var value: int = 1;")).parse();
  assert(kyna::validate(valid).empty());

  auto invalid = kyna::Parser(kyna::lex("const value = 1; value = 2;")).parse();
  auto diagnostics = kyna::validate(invalid);
  assert(!diagnostics.empty());
  assert(!diagnostics.front().warning);

  auto duplicate = kyna::Parser(kyna::lex("var value = 1; var value = 2;")).parse();
  auto duplicateDiagnostics = kyna::validate(duplicate);
  assert(std::any_of(duplicateDiagnostics.begin(), duplicateDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1102"; }));

  auto duplicateParameter =
      kyna::Parser(kyna::lex("fn add(value: int, value: int): int { return value; }")).parse();
  auto parameterDiagnostics = kyna::validate(duplicateParameter);
  assert(std::any_of(parameterDiagnostics.begin(), parameterDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1104"; }));

  auto wrongCall =
      kyna::Parser(kyna::lex("fn add(value: int): int { return value; } add();")).parse();
  auto callDiagnostics = kyna::validate(wrongCall);
  assert(std::any_of(callDiagnostics.begin(), callDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1201"; }));

  auto wrongBuiltinArity = kyna::Parser(kyna::lex("textSlice(\"Kyna\");")).parse();
  auto builtinArityDiagnostics = kyna::validate(wrongBuiltinArity);
  assert(std::any_of(builtinArityDiagnostics.begin(), builtinArityDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1204"; }));

  auto wrongBuiltinType = kyna::Parser(kyna::lex("textUpper(42);")).parse();
  auto builtinTypeDiagnostics = kyna::validate(wrongBuiltinType);
  assert(std::any_of(builtinTypeDiagnostics.begin(), builtinTypeDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1205"; }));

  auto outsideBreak = kyna::Parser(kyna::lex("break;")).parse();
  auto outsideBreakDiagnostics = kyna::validate(outsideBreak);
  assert(std::any_of(outsideBreakDiagnostics.begin(), outsideBreakDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1301"; }));

  auto unknownLabel = kyna::Parser(kyna::lex("loop { break missing; }")).parse();
  auto unknownLabelDiagnostics = kyna::validate(unknownLabel);
  assert(std::any_of(unknownLabelDiagnostics.begin(), unknownLabelDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1302"; }));

  auto invalidCondition = kyna::Parser(kyna::lex("while (42) { break; }")).parse();
  auto conditionDiagnostics = kyna::validate(invalidCondition);
  assert(std::any_of(conditionDiagnostics.begin(), conditionDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1304"; }));

  auto nonExhaustiveMatch =
      kyna::Parser(kyna::lex("const value = match (1) { 1 => \"one\"; };")).parse();
  auto matchDiagnostics = kyna::validate(nonExhaustiveMatch);
  assert(std::any_of(matchDiagnostics.begin(), matchDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1401"; }));

  auto duplicateMatch = kyna::Parser(
                            kyna::lex("const value = match (1) { 1 => \"one\"; 1 => \"again\"; "
                                       "_ => \"other\"; };"))
                            .parse();
  auto duplicateMatchDiagnostics = kyna::validate(duplicateMatch);
  assert(std::any_of(duplicateMatchDiagnostics.begin(), duplicateMatchDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1403"; }));

  auto scopedIfExpression = kyna::Parser(kyna::lex(
                                      "const value: str = if (true) { var prefix = \"ad\"; "
                                      "prefix + \"ult\" } else { \"minor\" };"))
                                .parse();
  assert(kyna::validate(scopedIfExpression).empty());

  auto nonCallable = kyna::Parser(kyna::lex("const value = 42; value();")).parse();
  auto nonCallableDiagnostics = kyna::validate(nonCallable);
  assert(std::any_of(nonCallableDiagnostics.begin(), nonCallableDiagnostics.end(),
                     [](const auto &diagnostic) { return diagnostic.code == "KSEM1203"; }));

  auto typedError = kyna::Parser(kyna::lex(
      "fn guarded(): str { try { throw \"failure\"; } catch (failure) { "
      "return failure.code + failure.message; } finally { print(\"cleanup\"); } }"))
                        .parse();
  assert(kyna::validate(typedError).empty());

  // Interface inheritance with generic substitution.
  auto intfExtends = kyna::Parser(kyna::lex(
                              "intf Base<T> { value: T; } "
                              "intf Child extends Base<int> { extra?: str; } "
                              "fn f(): int { return 1; }"))
                         .parse();
  assert(kyna::validate(intfExtends).empty());

  // Generic interface instantiation via implements.
  auto genericImplements =
      kyna::Parser(kyna::lex("intf Container<T> { put(item: T): void; } "
                             "class Box implements Container<int> { "
                             "public fn put(item: int): void { } }"))
          .parse();
  assert(kyna::validate(genericImplements).empty());

  // Optional property is not required of implements.
  auto optionalImplements = kyna::Parser(kyna::lex(
                                "intf WithOptional { required: int; optional?: str; } "
                                "class Slim implements WithOptional { required: int = 0; }"))
                                .parse();
  assert(kyna::validate(optionalImplements).empty());

  // Named/default/namespace import declarations parse and validate cleanly.
  auto jsImports = kyna::Parser(kyna::lex(
                         "import { a, b } from \"./m.kyna\"; "
                         "import d from \"./m.kyna\"; "
                         "import * as ns from \"./m.kyna\";"))
                       .parse();
  assert(kyna::validate(jsImports).empty());

  // Array literals retain their inferred element type through indexed access.
  auto typedArray =
      kyna::Parser(kyna::lex("const values = [1, 2, 3]; const first: int = values[0];"))
          .parse();
  assert(kyna::validate(typedArray).empty());

  auto wrongArrayElement =
      kyna::Parser(kyna::lex("const values = [1, 2, 3]; const first: str = values[0];"))
          .parse();
  assert(!kyna::validate(wrongArrayElement).empty());
}
