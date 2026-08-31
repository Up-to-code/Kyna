#include "kyna/bytecode/bytecode_disassembler.hpp"
#include "kyna/bytecode/program_bytecode_compiler.hpp"
#include "kyna/bytecode/bytecode_validator.hpp"
#include "kyna/execution/bytecode_virtual_machine.hpp"
#include "kyna/hir/hir_renderer.hpp"
#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/mir/hir_lowering.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/source/source_manager.hpp"
#include <cassert>
#include <string>

namespace {
class DeterministicNativeAdapter final : public kyna::BytecodeNativeAdapter {
public:
  kyna::NativeCallResult invoke(std::string_view name,
                                std::span<const kyna::RuntimeValue> arguments,
                                kyna::Heap &) override {
    if (name == "nativeAdd" && arguments.size() == 2)
      return {kyna::RuntimeValue(std::get<std::int64_t>(arguments[0].data) +
                                std::get<std::int64_t>(arguments[1].data)),
              std::nullopt};
    return {{}, kyna::NativeCallFailure{"KTEST9001", "deterministic native failure", {}}};
  }
};
} // namespace

int main() {
  kyna::SourceManager sources;
  const auto source = sources.add("pipeline", "set left = 20; set right = 22; return left + right;");
  auto lexed = kyna::tokenize(*sources.find(source));
  auto parsed = kyna::parseModule(*sources.find(source), std::move(lexed.tokens));
  auto hir = kyna::lowerSyntaxToHir("pipeline", parsed.tree);
  assert(hir.ok());
  auto mir = kyna::lowerHirToMir(*hir.program);
  assert(mir.ok());
  auto pipelineModule = kyna::compileMirToBytecode(*mir.program);
  assert(pipelineModule.ok());
  const auto pipelineResult = kyna::BytecodeVirtualMachine().execute(*pipelineModule.module);
  assert(pipelineResult.ok());
  assert(std::get<std::int64_t>(pipelineResult.value.data) == 42);

  const auto controlSource = sources.add(
      "control-flow",
      "let value = 0; while (value < 3) { value = value + 1; } if (value == 3) { return value; } else { return 0; }");
  auto controlLexed = kyna::tokenize(*sources.find(controlSource));
  auto controlParsed =
      kyna::parseModule(*sources.find(controlSource), std::move(controlLexed.tokens));
  auto controlHir = kyna::lowerSyntaxToHir("control-flow", controlParsed.tree);
  assert(controlHir.ok());
  auto controlMir = kyna::lowerHirToMir(*controlHir.program);
  assert(controlMir.ok());
  auto controlModule = kyna::compileMirToBytecode(*controlMir.program);
  assert(controlModule.ok());
  const auto controlResult = kyna::BytecodeVirtualMachine().execute(*controlModule.module);
  assert(controlResult.ok());
  assert(std::get<std::int64_t>(controlResult.value.data) == 3);

  const auto recursiveSource = sources.add(
      "recursive",
      "func factorial(n: int): int { if (n <= 1) { return 1; } else { return n * factorial(n - 1); } } return factorial(6);");
  auto recursiveLexed = kyna::tokenize(*sources.find(recursiveSource));
  auto recursiveParsed =
      kyna::parseModule(*sources.find(recursiveSource), std::move(recursiveLexed.tokens));
  auto recursiveHir = kyna::lowerSyntaxToHir("recursive", recursiveParsed.tree);
  assert(recursiveHir.ok());
  auto recursiveMir = kyna::lowerHirToMir(*recursiveHir.program);
  assert(recursiveMir.ok());
  auto recursiveModule = kyna::compileMirToBytecode(*recursiveMir.program);
  assert(recursiveModule.ok());
  const auto recursiveResult = kyna::BytecodeVirtualMachine().execute(*recursiveModule.module);
  assert(recursiveResult.ok());
  assert(std::get<std::int64_t>(recursiveResult.value.data) == 720);

  const auto failingSource = sources.add(
      "call-stack",
      "func divide(value: int): float { return 10 / value; } return divide(0);");
  auto failingLexed = kyna::tokenize(*sources.find(failingSource));
  auto failingParsed =
      kyna::parseModule(*sources.find(failingSource), std::move(failingLexed.tokens));
  auto failingHir = kyna::lowerSyntaxToHir("call-stack", failingParsed.tree);
  assert(failingHir.ok());
  auto failingMir = kyna::lowerHirToMir(*failingHir.program);
  assert(failingMir.ok());
  auto failingModule = kyna::compileMirToBytecode(*failingMir.program);
  assert(failingModule.ok());
  const auto failingResult = kyna::BytecodeVirtualMachine().execute(*failingModule.module);
  assert(!failingResult.ok());
  assert(failingResult.diagnostics.front().code == "KRT2201");
  assert(failingResult.diagnostics.front().callFrames.size() == 2);
  assert(failingResult.diagnostics.front().callFrames.front().function == "divide");

  const auto overflowSource =
      sources.add("overflow", "return 9223372036854775807 + 1;");
  auto overflowLexed = kyna::tokenize(*sources.find(overflowSource));
  auto overflowParsed =
      kyna::parseModule(*sources.find(overflowSource), std::move(overflowLexed.tokens));
  auto overflowHir = kyna::lowerSyntaxToHir("overflow", overflowParsed.tree);
  assert(overflowHir.ok());
  auto overflowMir = kyna::lowerHirToMir(*overflowHir.program);
  assert(overflowMir.ok());
  auto overflowModule = kyna::compileMirToBytecode(*overflowMir.program);
  assert(overflowModule.ok());
  const auto overflowResult = kyna::BytecodeVirtualMachine().execute(*overflowModule.module);
  assert(!overflowResult.ok());
  assert(overflowResult.diagnostics.front().code == "KRT2204");

  const auto stringSource = sources.add("string-add", "return 42 + \" answers\";");
  auto stringLexed = kyna::tokenize(*sources.find(stringSource));
  auto stringParsed =
      kyna::parseModule(*sources.find(stringSource), std::move(stringLexed.tokens));
  auto stringHir = kyna::lowerSyntaxToHir("string-add", stringParsed.tree);
  assert(stringHir.ok());
  auto stringMir = kyna::lowerHirToMir(*stringHir.program);
  assert(stringMir.ok());
  auto stringModule = kyna::compileMirToBytecode(*stringMir.program);
  assert(stringModule.ok());
  const auto stringResult = kyna::BytecodeVirtualMachine().execute(*stringModule.module);
  assert(stringResult.ok());
  assert(std::get<std::string>(stringResult.value.data) == "42 answers");

  const auto operatorSource = sources.add(
      "operators",
      "if (!(3 >= 4)) { if (3 != 4) { return -5 + 8 % 3; } } return 0;");
  auto operatorLexed = kyna::tokenize(*sources.find(operatorSource));
  auto operatorParsed =
      kyna::parseModule(*sources.find(operatorSource), std::move(operatorLexed.tokens));
  auto operatorHir = kyna::lowerSyntaxToHir("operators", operatorParsed.tree);
  assert(operatorHir.ok());
  auto operatorMir = kyna::lowerHirToMir(*operatorHir.program);
  assert(operatorMir.ok());
  auto operatorModule = kyna::compileMirToBytecode(*operatorMir.program);
  assert(operatorModule.ok());
  const auto operatorResult = kyna::BytecodeVirtualMachine().execute(*operatorModule.module);
  assert(operatorResult.ok());
  assert(std::get<std::int64_t>(operatorResult.value.data) == -3);

  const auto remainderSource = sources.add("remainder-zero", "return 5 % 0;");
  auto remainderLexed = kyna::tokenize(*sources.find(remainderSource));
  auto remainderParsed =
      kyna::parseModule(*sources.find(remainderSource), std::move(remainderLexed.tokens));
  auto remainderHir = kyna::lowerSyntaxToHir("remainder-zero", remainderParsed.tree);
  assert(remainderHir.ok());
  auto remainderMir = kyna::lowerHirToMir(*remainderHir.program);
  assert(remainderMir.ok());
  auto remainderModule = kyna::compileMirToBytecode(*remainderMir.program);
  assert(remainderModule.ok());
  const auto remainderResult = kyna::BytecodeVirtualMachine().execute(*remainderModule.module);
  assert(!remainderResult.ok());
  assert(remainderResult.diagnostics.front().code == "KRT2201");

  const auto shortCircuitSource = sources.add(
      "short-circuit",
      "if (false && (1 / 0 == 0)) { return 1; } "
      "if (true || (1 / 0 == 0)) { return 42; } return 0;");
  auto shortCircuitLexed = kyna::tokenize(*sources.find(shortCircuitSource));
  auto shortCircuitParsed = kyna::parseModule(*sources.find(shortCircuitSource),
                                               std::move(shortCircuitLexed.tokens));
  auto shortCircuitHir = kyna::lowerSyntaxToHir("short-circuit", shortCircuitParsed.tree);
  assert(shortCircuitHir.ok());
  auto shortCircuitMir = kyna::lowerHirToMir(*shortCircuitHir.program);
  assert(shortCircuitMir.ok());
  auto shortCircuitModule = kyna::compileMirToBytecode(*shortCircuitMir.program);
  assert(shortCircuitModule.ok());
  const auto shortCircuitResult =
      kyna::BytecodeVirtualMachine().execute(*shortCircuitModule.module);
  assert(shortCircuitResult.ok());
  assert(std::get<std::int64_t>(shortCircuitResult.value.data) == 42);

  const auto loopSource = sources.add(
      "loop-control",
      "let total = 0; outer: loop (let i = 0; i < 5; i = i + 1) { "
      "loop (let j = 0; j < 5; j = j + 1) { "
      "if (j == 1) { continue; } if (i == 3) { break outer; } total = total + 1; "
      "} } return total;");
  auto loopLexed = kyna::tokenize(*sources.find(loopSource));
  auto loopParsed = kyna::parseModule(*sources.find(loopSource), std::move(loopLexed.tokens));
  auto loopHir = kyna::lowerSyntaxToHir("loop-control", loopParsed.tree);
  assert(loopHir.ok());
  auto loopMir = kyna::lowerHirToMir(*loopHir.program);
  assert(loopMir.ok());
  auto loopModule = kyna::compileMirToBytecode(*loopMir.program);
  assert(loopModule.ok());
  const auto loopResult = kyna::BytecodeVirtualMachine().execute(*loopModule.module);
  assert(loopResult.ok());
  assert(std::get<std::int64_t>(loopResult.value.data) == 12);

  const auto ifExpressionSource = sources.add(
      "if-expression",
      "set age = 21; set category = if (age >= 18) { let prefix = \"ad\"; prefix + \"ult\" } "
      "else { \"minor\" }; return category;");
  auto ifExpressionLexed = kyna::tokenize(*sources.find(ifExpressionSource));
  auto ifExpressionParsed = kyna::parseModule(*sources.find(ifExpressionSource),
                                               std::move(ifExpressionLexed.tokens));
  auto ifExpressionHir = kyna::lowerSyntaxToHir("if-expression", ifExpressionParsed.tree);
  assert(ifExpressionHir.ok());
  auto ifExpressionMir = kyna::lowerHirToMir(*ifExpressionHir.program);
  assert(ifExpressionMir.ok());
  auto ifExpressionModule = kyna::compileMirToBytecode(*ifExpressionMir.program);
  assert(ifExpressionModule.ok());
  const auto ifExpressionResult =
      kyna::BytecodeVirtualMachine().execute(*ifExpressionModule.module);
  assert(ifExpressionResult.ok());
  assert(std::get<std::string>(ifExpressionResult.value.data) == "adult");

  const auto matchSource = sources.add(
      "match-expression",
      "func label(value: int): str { return match (value) { 0 => \"zero\"; 1 => \"one\"; "
      "_ => \"other\"; }; } return label(1);");
  auto matchLexed = kyna::tokenize(*sources.find(matchSource));
  auto matchParsed = kyna::parseModule(*sources.find(matchSource), std::move(matchLexed.tokens));
  auto matchHir = kyna::lowerSyntaxToHir("match-expression", matchParsed.tree);
  assert(matchHir.ok());
  assert(kyna::renderHir(*matchHir.program).find("match") != std::string::npos);
  auto matchMir = kyna::lowerHirToMir(*matchHir.program);
  assert(matchMir.ok());
  auto matchModule = kyna::compileMirToBytecode(*matchMir.program);
  assert(matchModule.ok());
  const auto matchResult = kyna::BytecodeVirtualMachine().execute(*matchModule.module);
  assert(matchResult.ok());
  assert(std::get<std::string>(matchResult.value.data) == "one");

  const auto firstClassSource = sources.add(
      "first-class-functions",
      "func add(left: int, right: int): int { return left + right; } "
      "func apply(operation: any, left: int, right: int): any { "
      "return operation(left, right); } set selected = add; return apply(selected, 20, 22);");
  auto firstClassLexed = kyna::tokenize(*sources.find(firstClassSource));
  auto firstClassParsed =
      kyna::parseModule(*sources.find(firstClassSource), std::move(firstClassLexed.tokens));
  auto firstClassHir =
      kyna::lowerSyntaxToHir("first-class-functions", firstClassParsed.tree);
  assert(firstClassHir.ok());
  auto firstClassMir = kyna::lowerHirToMir(*firstClassHir.program);
  assert(firstClassMir.ok());
  auto firstClassModule = kyna::compileMirToBytecode(*firstClassMir.program);
  assert(firstClassModule.ok());
  assert(firstClassModule.module->formatVersion == 5);
  const auto firstClassListing = kyna::disassembleBytecode(*firstClassModule.module);
  assert(firstClassListing.find("load.function") != std::string::npos);
  assert(firstClassListing.find("call.indirect") != std::string::npos);
  const auto firstClassResult =
      kyna::BytecodeVirtualMachine().execute(*firstClassModule.module);
  assert(firstClassResult.ok());
  assert(std::get<std::int64_t>(firstClassResult.value.data) == 42);

  const auto closureSource = sources.add(
      "closures",
      "func makeAdder(base: int): any { func add(value: int): int { return base + value; } "
      "return add; } "
      "func makeCounter(start: int): any { let value = start; func next(): int { "
      "value = value + 1; return value; } return next; } "
      "func makeFactorial(): any { func factorial(value: int): int { "
      "if (value <= 1) { return 1; } return value * factorial(value - 1); } "
      "return factorial; } "
      "func makeNested(base: int): any { func middle(): any { "
      "func inner(value: int): int { return base + value; } return inner; } "
      "return middle(); } "
      "set addForty = makeAdder(40); set first = makeCounter(0); "
      "set second = makeCounter(10); set factorial = makeFactorial(); "
      "set nested = makeNested(40); "
      "first(); first(); second(); "
      "return addForty(2) + first() + second() + factorial(5) + nested(2);");
  auto closureLexed = kyna::tokenize(*sources.find(closureSource));
  auto closureParsed =
      kyna::parseModule(*sources.find(closureSource), std::move(closureLexed.tokens));
  auto closureHir = kyna::lowerSyntaxToHir("closures", closureParsed.tree);
  assert(closureHir.ok());
  auto closureMir = kyna::lowerHirToMir(*closureHir.program);
  assert(closureMir.ok());
  auto closureModule = kyna::compileMirToBytecode(*closureMir.program);
  assert(closureModule.ok());
  const auto closureListing = kyna::disassembleBytecode(*closureModule.module);
  assert(closureListing.find("make.closure") != std::string::npos);
  assert(closureListing.find("load.capture") != std::string::npos);
  assert(closureListing.find("store.capture") != std::string::npos);
  const auto closureResult =
      kyna::BytecodeVirtualMachine().execute(*closureModule.module);
  assert(closureResult.ok());
  assert(std::get<std::int64_t>(closureResult.value.data) == 219);

  const auto closureGcSource = sources.add(
      "closure-gc",
      "func churn(): int { let sum = 0; loop (let i = 0; i < 600; i = i + 1) { "
      "func current(): int { return i; } sum = sum + current(); } return sum; } "
      "return churn();");
  auto closureGcLexed = kyna::tokenize(*sources.find(closureGcSource));
  auto closureGcParsed =
      kyna::parseModule(*sources.find(closureGcSource), std::move(closureGcLexed.tokens));
  auto closureGcHir = kyna::lowerSyntaxToHir("closure-gc", closureGcParsed.tree);
  assert(closureGcHir.ok());
  auto closureGcMir = kyna::lowerHirToMir(*closureGcHir.program);
  assert(closureGcMir.ok());
  auto closureGcModule = kyna::compileMirToBytecode(*closureGcMir.program);
  assert(closureGcModule.ok());
  const auto closureGcResult =
      kyna::BytecodeVirtualMachine().execute(*closureGcModule.module);
  assert(closureGcResult.ok());
  assert(std::get<std::int64_t>(closureGcResult.value.data) == 179700);
  assert(closureGcResult.heapStats.collections > 0);
  assert(closureGcResult.heapStats.reclaimed > 0);

  const auto nonCallableSource = sources.add("non-callable", "set value = 42; return value();");
  auto nonCallableLexed = kyna::tokenize(*sources.find(nonCallableSource));
  auto nonCallableParsed =
      kyna::parseModule(*sources.find(nonCallableSource), std::move(nonCallableLexed.tokens));
  auto nonCallableHir = kyna::lowerSyntaxToHir("non-callable", nonCallableParsed.tree);
  assert(nonCallableHir.ok());
  auto nonCallableMir = kyna::lowerHirToMir(*nonCallableHir.program);
  assert(nonCallableMir.ok());
  auto nonCallableModule = kyna::compileMirToBytecode(*nonCallableMir.program);
  assert(nonCallableModule.ok());
  const auto nonCallableResult =
      kyna::BytecodeVirtualMachine().execute(*nonCallableModule.module);
  assert(!nonCallableResult.ok());
  assert(nonCallableResult.diagnostics.front().code == "KVM2010");

  const auto indirectAritySource = sources.add(
      "indirect-arity",
      "func add(left: int, right: int): int { return left + right; } "
      "set selected = add; return selected(1);");
  auto indirectArityLexed = kyna::tokenize(*sources.find(indirectAritySource));
  auto indirectArityParsed = kyna::parseModule(*sources.find(indirectAritySource),
                                                std::move(indirectArityLexed.tokens));
  auto indirectArityHir =
      kyna::lowerSyntaxToHir("indirect-arity", indirectArityParsed.tree);
  assert(indirectArityHir.ok());
  auto indirectArityMir = kyna::lowerHirToMir(*indirectArityHir.program);
  assert(indirectArityMir.ok());
  auto indirectArityModule = kyna::compileMirToBytecode(*indirectArityMir.program);
  assert(indirectArityModule.ok());
  const auto indirectArityResult =
      kyna::BytecodeVirtualMachine().execute(*indirectArityModule.module);
  assert(!indirectArityResult.ok());
  assert(indirectArityResult.diagnostics.front().code == "KVM2011");

  const auto exceptionSource = sources.add(
      "bytecode-exceptions",
      "let marker = 0; try { throw \"boom\"; } catch (failure) { marker = 40; } "
      "finally { marker = marker + 2; } return marker;");
  auto exceptionLexed = kyna::tokenize(*sources.find(exceptionSource));
  auto exceptionParsed =
      kyna::parseModule(*sources.find(exceptionSource), std::move(exceptionLexed.tokens));
  auto exceptionHir = kyna::lowerSyntaxToHir("bytecode-exceptions", exceptionParsed.tree);
  assert(exceptionHir.ok());
  auto exceptionMir = kyna::lowerHirToMir(*exceptionHir.program);
  assert(exceptionMir.ok());
  auto exceptionModule = kyna::compileMirToBytecode(*exceptionMir.program);
  assert(exceptionModule.ok());
  const auto exceptionListing = kyna::disassembleBytecode(*exceptionModule.module);
  assert(exceptionListing.find("throw") != std::string::npos);
  assert(exceptionListing.find("exception [") != std::string::npos);
  const auto exceptionResult =
      kyna::BytecodeVirtualMachine().execute(*exceptionModule.module);
  assert(exceptionResult.ok());
  assert(std::get<std::int64_t>(exceptionResult.value.data) == 42);

  const auto crossFrameSource = sources.add(
      "cross-frame-exceptions",
      "func fail(): void { throw \"cross-frame\"; } "
      "try { fail(); return 0; } catch (failure) { return 42; }");
  auto crossFrameLexed = kyna::tokenize(*sources.find(crossFrameSource));
  auto crossFrameParsed =
      kyna::parseModule(*sources.find(crossFrameSource), std::move(crossFrameLexed.tokens));
  auto crossFrameHir = kyna::lowerSyntaxToHir("cross-frame-exceptions", crossFrameParsed.tree);
  assert(crossFrameHir.ok());
  auto crossFrameMir = kyna::lowerHirToMir(*crossFrameHir.program);
  assert(crossFrameMir.ok());
  auto crossFrameModule = kyna::compileMirToBytecode(*crossFrameMir.program);
  assert(crossFrameModule.ok());
  const auto crossFrameResult =
      kyna::BytecodeVirtualMachine().execute(*crossFrameModule.module);
  assert(crossFrameResult.ok());
  assert(std::get<std::int64_t>(crossFrameResult.value.data) == 42);

  const auto uncaughtSource = sources.add(
      "uncaught-exception",
      "func fail(): void { throw \"uncaught\"; } fail();");
  auto uncaughtLexed = kyna::tokenize(*sources.find(uncaughtSource));
  auto uncaughtParsed =
      kyna::parseModule(*sources.find(uncaughtSource), std::move(uncaughtLexed.tokens));
  auto uncaughtHir = kyna::lowerSyntaxToHir("uncaught-exception", uncaughtParsed.tree);
  assert(uncaughtHir.ok());
  auto uncaughtMir = kyna::lowerHirToMir(*uncaughtHir.program);
  assert(uncaughtMir.ok());
  auto uncaughtModule = kyna::compileMirToBytecode(*uncaughtMir.program);
  assert(uncaughtModule.ok());
  const auto uncaughtResult = kyna::BytecodeVirtualMachine().execute(*uncaughtModule.module);
  assert(!uncaughtResult.ok());
  assert(uncaughtResult.diagnostics.front().code == "KVM2301");
  assert(uncaughtResult.diagnostics.front().callFrames.size() == 2);

  const auto caughtRuntimeSource = sources.add(
      "caught-runtime-error",
      "try { set impossible = 1 / 0; return 0; } "
      "catch (failure) { return 42; }");
  auto caughtRuntimeLexed = kyna::tokenize(*sources.find(caughtRuntimeSource));
  auto caughtRuntimeParsed = kyna::parseModule(*sources.find(caughtRuntimeSource),
                                                std::move(caughtRuntimeLexed.tokens));
  auto caughtRuntimeHir =
      kyna::lowerSyntaxToHir("caught-runtime-error", caughtRuntimeParsed.tree);
  assert(caughtRuntimeHir.ok());
  auto caughtRuntimeMir = kyna::lowerHirToMir(*caughtRuntimeHir.program);
  assert(caughtRuntimeMir.ok());
  auto caughtRuntimeModule = kyna::compileMirToBytecode(*caughtRuntimeMir.program);
  assert(caughtRuntimeModule.ok());
  const auto caughtRuntimeResult =
      kyna::BytecodeVirtualMachine().execute(*caughtRuntimeModule.module);
  assert(caughtRuntimeResult.ok());
  assert(std::get<std::int64_t>(caughtRuntimeResult.value.data) == 42);

  const auto inspectedErrorSource = sources.add(
      "inspected-error",
      "try { set impossible = 1 / 0; return \"missed\"; } "
      "catch (failure) { return failure.code + \"::\" + failure.message; }");
  auto inspectedErrorLexed = kyna::tokenize(*sources.find(inspectedErrorSource));
  auto inspectedErrorParsed = kyna::parseModule(*sources.find(inspectedErrorSource),
                                                 std::move(inspectedErrorLexed.tokens));
  auto inspectedErrorHir =
      kyna::lowerSyntaxToHir("inspected-error", inspectedErrorParsed.tree);
  assert(inspectedErrorHir.ok());
  auto inspectedErrorMir = kyna::lowerHirToMir(*inspectedErrorHir.program);
  assert(inspectedErrorMir.ok());
  auto inspectedErrorModule = kyna::compileMirToBytecode(*inspectedErrorMir.program);
  assert(inspectedErrorModule.ok());
  const auto inspectedErrorListing =
      kyna::disassembleBytecode(*inspectedErrorModule.module);
  assert(inspectedErrorListing.find("load.member") != std::string::npos);
  const auto inspectedErrorResult =
      kyna::BytecodeVirtualMachine().execute(*inspectedErrorModule.module);
  assert(inspectedErrorResult.ok());
  assert(std::get<std::string>(inspectedErrorResult.value.data) ==
         "KRT2201::division by zero");

  const auto nativeSource = sources.add(
      "native-call", "return nativeAdd(20, 22);");
  auto nativeLexed = kyna::tokenize(*sources.find(nativeSource));
  auto nativeParsed =
      kyna::parseModule(*sources.find(nativeSource), std::move(nativeLexed.tokens));
  auto nativeHir = kyna::lowerSyntaxToHir(
      "native-call", nativeParsed.tree, kyna::HirLoweringOptions{{"nativeAdd"}});
  assert(nativeHir.ok());
  assert(kyna::renderHir(*nativeHir.program).find("call.native nativeAdd") !=
         std::string::npos);
  auto nativeMir = kyna::lowerHirToMir(*nativeHir.program);
  assert(nativeMir.ok());
  auto nativeModule = kyna::compileMirToBytecode(*nativeMir.program);
  assert(nativeModule.ok());
  assert(kyna::disassembleBytecode(*nativeModule.module).find("call.native") !=
         std::string::npos);
  DeterministicNativeAdapter nativeAdapter;
  const auto nativeResult =
      kyna::BytecodeVirtualMachine().execute(*nativeModule.module, &nativeAdapter);
  assert(nativeResult.ok());
  assert(std::get<std::int64_t>(nativeResult.value.data) == 42);

  const auto missingNativeResult =
      kyna::BytecodeVirtualMachine().execute(*nativeModule.module);
  assert(!missingNativeResult.ok());
  assert(missingNativeResult.diagnostics.front().code == "KVM2021");

  const auto collectionLiteralSource = sources.add(
      "collection-literals",
      "set values = [20, 22]; set record = { answer: values[0] + values[1] }; "
      "return record.answer;");
  auto collectionLiteralLexed = kyna::tokenize(*sources.find(collectionLiteralSource));
  auto collectionLiteralParsed = kyna::parseModule(*sources.find(collectionLiteralSource),
                                                    std::move(collectionLiteralLexed.tokens));
  auto collectionLiteralHir =
      kyna::lowerSyntaxToHir("collection-literals", collectionLiteralParsed.tree);
  assert(collectionLiteralHir.ok());
  auto collectionLiteralMir = kyna::lowerHirToMir(*collectionLiteralHir.program);
  assert(collectionLiteralMir.ok());
  auto collectionLiteralModule = kyna::compileMirToBytecode(*collectionLiteralMir.program);
  assert(collectionLiteralModule.ok());
  const auto collectionLiteralListing =
      kyna::disassembleBytecode(*collectionLiteralModule.module);
  assert(collectionLiteralListing.find("make.array") != std::string::npos);
  assert(collectionLiteralListing.find("make.object") != std::string::npos);
  assert(collectionLiteralListing.find("load.index") != std::string::npos);
  const auto collectionLiteralResult =
      kyna::BytecodeVirtualMachine().execute(*collectionLiteralModule.module);
  assert(collectionLiteralResult.ok());
  assert(std::get<std::int64_t>(collectionLiteralResult.value.data) == 42);

  const auto collectionMutationSource = sources.add(
      "collection-mutation",
      "let values = [1]; values[0] = 40; let record = { answer: 0 }; "
      "record.answer = values[0] + 2; return record[\"answer\"];");
  auto collectionMutationLexed = kyna::tokenize(*sources.find(collectionMutationSource));
  auto collectionMutationParsed = kyna::parseModule(*sources.find(collectionMutationSource),
                                                     std::move(collectionMutationLexed.tokens));
  auto collectionMutationHir =
      kyna::lowerSyntaxToHir("collection-mutation", collectionMutationParsed.tree);
  assert(collectionMutationHir.ok());
  auto collectionMutationMir = kyna::lowerHirToMir(*collectionMutationHir.program);
  assert(collectionMutationMir.ok());
  auto collectionMutationModule = kyna::compileMirToBytecode(*collectionMutationMir.program);
  assert(collectionMutationModule.ok());
  const auto collectionMutationListing =
      kyna::disassembleBytecode(*collectionMutationModule.module);
  assert(collectionMutationListing.find("store.index") != std::string::npos);
  assert(collectionMutationListing.find("store.member") != std::string::npos);
  const auto collectionMutationResult =
      kyna::BytecodeVirtualMachine().execute(*collectionMutationModule.module);
  assert(collectionMutationResult.ok());
  assert(std::get<std::int64_t>(collectionMutationResult.value.data) == 42);

  kyna::BytecodeModule module;
  module.name = "arithmetic";
  module.constants = {std::int64_t{20}, std::int64_t{22}};
  module.functions.push_back({"main", 3,
                              {{kyna::OpCode::LoadConstant, 0, 0, 0, {1, 1}},
                               {kyna::OpCode::LoadConstant, 1, 1, 0, {1, 6}},
                               {kyna::OpCode::Add, 2, 0, 1, {1, 4}},
                               {kyna::OpCode::Return, 0, 2, 0, {1, 9}}},
                              0, 0, {}});

  assert(kyna::validateBytecode(module).ok());
  const auto listing = kyna::disassembleBytecode(module);
  assert(listing.find("load.constant") != std::string::npos);
  assert(listing.find("add") != std::string::npos);

  const auto executed = kyna::BytecodeVirtualMachine().execute(module);
  assert(executed.ok());
  assert(std::get<std::int64_t>(executed.value.data) == 42);

  module.functions.front().instructions.front().first = 99;
  const auto invalid = kyna::validateBytecode(module);
  assert(!invalid.ok());
  assert(invalid.diagnostics.front().code == "KBC1104");
}
