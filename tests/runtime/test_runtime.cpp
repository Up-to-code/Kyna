#include "kyna/execution/tree_walk_engine.hpp"
#include "kyna/parsing/recursive_descent_parser.hpp"
#include "kyna/semantics/program_validation.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"
#include <cassert>

int main() {
  auto program =
      kyna::Parser(kyna::lex("class Node { public next: Node?; }"
                             "func make(): void { let n = new Node(); n.next = n; return; }"
                             "loop (let i = 0; i < 300; i = i + 1) { make(); }"))
          .parse();
  assert(kyna::validate(program).empty());
  kyna::Interpreter interpreter(kyna::productionRuntimeCapabilities(),
                                kyna::installStandardLibrary);
  auto baselineObjects = interpreter.heap().live();
  interpreter.heap().setThreshold(1);
  interpreter.execute(program);
  assert(interpreter.heap().collections() > 0);
  assert(interpreter.heap().live() == baselineObjects);

  kyna::Heap isolatedHeap;
  {
    auto roots = isolatedHeap.rootScope();
    kyna::Value temporary(isolatedHeap.allocate());
    roots.protect(temporary);
    isolatedHeap.collect({});
    assert(isolatedHeap.live() == 1);
  }
  isolatedHeap.collect({});
  assert(isolatedHeap.live() == 0);

  {
    auto roots = isolatedHeap.rootScope();
    auto *closure = isolatedHeap.allocateClosure(1, {});
    auto *recursiveCell = isolatedHeap.allocateCaptureCell(kyna::Value(closure));
    closure->captures.push_back(recursiveCell);
    kyna::Value closureRoot(closure);
    roots.protect(closureRoot);
    isolatedHeap.collect({});
    assert(isolatedHeap.live() == 2);
  }
  isolatedHeap.collect({});
  assert(isolatedHeap.live() == 0);

  {
    auto roots = isolatedHeap.rootScope();
    auto *receiver = isolatedHeap.allocate();
    auto *method = isolatedHeap.allocateBoundMethod(receiver, 7);
    kyna::Value methodRoot(method);
    roots.protect(methodRoot);
    isolatedHeap.collect({});
    assert(isolatedHeap.live() == 2);
  }
  isolatedHeap.collect({});
  assert(isolatedHeap.live() == 0);
}
