#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include "kyna/lexing/legacy_lexer.hpp"
#include "kyna/parsing/recursive_descent_parser.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"
#include <cassert>
#include <iostream>
#include <sstream>
using namespace kyna;
static void run(const std::string &s) {
  auto p = Parser(lex(s)).parse();
  auto e = Analyzer().analyze(p);
  assert(e.empty());
  Interpreter i(productionRuntimeCapabilities(), installStandardLibrary);
  i.execute(p);
}
int main() {
  auto ts = lex("var x: int = 2;");
  assert(ts[0].kind == TokenKind::Var);
  assert(ts[6].kind == TokenKind::Semicolon);
  assert(lex("let alias: int = 1;")[0].kind == TokenKind::Var);
  assert(lex("set alias: int = 1;")[0].kind == TokenKind::Const);
  assert(lex("func alias() {}")[0].kind == TokenKind::Fn);
  run("var x: int = 2; x = 3;");
  run("fn add(a: int, b: int): int { return a + b; } print(add(2, 3));");
  run("var total = 0; loop (var i = 0; i < 3; i = i + 1) { total = total + i; } print(total);");
  run("class Animal { public name: str; public init(name: str) { self.name = name; } public fn "
      "speak(): str { return self.name; } } var a = new Animal(\"cat\"); print(a.speak());");
  run("class A { public value: int; public init(value: int) { self.value = value; } public fn "
      "get(): int { return self.value; } } class B extends A { public override fn get(): int { "
      "return super.get() + 1; } } var b = new B(4); print(b.get());");
  run("var dynamic: any; dynamic = 1; dynamic = \"text\"; var maybe: str? = null; print(dynamic);");
  run("var obj = { name: \"Kyna\", version: 1 }; obj.name = \"Language\"; print(obj.name); "
      "print(len(keys(obj)));");
  run("var values = [1, 2, 3]; values[1] = 8; push(values, 4); print(len(values)); "
      "print(values[1]); print(pop(values));");
  run("print(processRun(\"true\"));");
  run("try { error(\"expected failure\"); } catch (message) { log(message); } "
      "console.log(\"console works\"); logColor(\"green\", \"colored\");");
  auto exceptionProgram = Parser(lex(
      "var trace = \"\"; try { trace = trace + \"try:\"; throw \"boom\"; } "
      "catch (failure) { trace = trace + failure.code + \":\" + failure.message + \":\" + "
      "failure.cause; } finally { trace = trace + \":finally-1\"; trace = trace + "
      "\":finally-2\"; } fn throughFinally(): str { try { return \"returned\"; } "
      "finally { trace = trace + \":return-cleanup\"; } } const returned = throughFinally();"))
                              .parse();
  assert(Analyzer().analyze(exceptionProgram).empty());
  Interpreter exceptionInterpreter(productionRuntimeCapabilities(), installStandardLibrary);
  exceptionInterpreter.execute(exceptionProgram);
  assert(exceptionInterpreter.currentEnvironment() == exceptionInterpreter.globals());
  assert(std::get<std::string>(exceptionInterpreter.globals()->get("trace").value.data) ==
         "try:KRT2301:boom:boom:finally-1:finally-2:return-cleanup");
  assert(std::get<std::string>(exceptionInterpreter.currentEnvironment()->get("returned").value.data) ==
         "returned");

auto uncaughtFinally = Parser(lex(
      "var cleanup = \"\"; try { throw \"uncaught\"; } finally { "
      "cleanup = cleanup + \"first\"; cleanup = cleanup + \"-second\"; }"))
                              .parse();
  assert(Analyzer().analyze(uncaughtFinally).empty());
  Interpreter uncaughtInterpreter(productionRuntimeCapabilities(), installStandardLibrary);
  bool observedUncaught = false;
  try {
    uncaughtInterpreter.execute(uncaughtFinally);
  } catch (const RuntimeThrownError &error) {
    observedUncaught = error.value && error.value->code == "KRT2301";
  }
  assert(observedUncaught);
  assert(std::get<std::string>(uncaughtInterpreter.globals()->get("cleanup").value.data) ==
         "first-second");
  run("var n = 2; const text = if (n == 2) { \"yes\" } else { \"no\" }; print(text); "
      "print(match (n) { 1 => \"one\"; 2 => \"two\"; _ => \"other\"; });");
  run("var score = 85; var grade: str = \"\"; if (score >= 90) { grade = \"A\"; } else if (score >= 80) "
      "{ grade = \"B\"; } else if (score >= 70) { grade = \"C\"; } else { grade = \"F\"; } "
      "print(grade);");
  run("var code = 404; var label: str = \"\"; switch (code) { case 200: { label = \"ok\"; } "
      "case 404: { label = \"missing\"; } default: { label = \"other\"; } } print(label);");
  run("var totalOuter = 0; switch (2) { case 1: { totalOuter = 1; } case 2: { "
      "loop (totalOuter = 0; totalOuter < 3; totalOuter = totalOuter + 1) { "
      "if (totalOuter == 1) { break; } } } default: { totalOuter = 99; } } print(totalOuter);");
  run("var awaited = await 42; print(awaited);");
  run("fn compute(): int { return 7; } var response = await compute(); print(response);");
  auto gcProgram =
      Parser(lex("class Node { public next: Node?; } fn make(): void { var n = new Node(); "
                 "n.next = n; return; } loop (var i = 0; i < 300; i = i + 1) { make(); }"))
          .parse();
  assert(Analyzer().analyze(gcProgram).empty());
  Interpreter gcInterpreter(productionRuntimeCapabilities(), installStandardLibrary);
  auto baselineObjects = gcInterpreter.heap().live();
  gcInterpreter.heap().setThreshold(1);
  gcInterpreter.execute(gcProgram);
  assert(gcInterpreter.heap().collections() > 0);
  assert(gcInterpreter.heap().live() == baselineObjects);
  auto bad = Parser(lex("var x: int = \"bad\";")).parse();
  assert(!Analyzer().analyze(bad).empty());
  std::cout << "all Kyna tests passed\n";
}
