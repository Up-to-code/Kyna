#include "kyna/semantics/symbol.hpp"
#include "kyna/semantics/scope.hpp"
#include "kyna/semantics/environment.hpp"
#include "kyna/semantics/cycle_detector.hpp"
#include "kyna/types/type.hpp"
#include "kyna/types/basic_type.hpp"

#include <cassert>
#include <string>

namespace {

using namespace kyna;
using namespace kyna::semantics;
using namespace kyna::types;

void test_scope_insert_and_local_lookup() {
  Scope root;
  auto x = std::make_shared<VarSymbol>("x", Universe::Int(), true);
  assert(root.insert(x) == nullptr); // fresh name
  assert(root.localCount() == 1);

  // Duplicate in the same scope returns the existing symbol.
  auto x2 = std::make_shared<VarSymbol>("x", Universe::Bool(), false);
  assert(root.insert(x2) == x);
  assert(root.localCount() == 1);
}

void test_scope_shadowing() {
  Scope outer;
  auto x = std::make_shared<VarSymbol>("x", Universe::Int(), true);
  outer.insert(x);

  auto *inner = outer.createChild();
  // Inner scope shadows the outer binding.
  auto innerX = std::make_shared<VarSymbol>("x", Universe::String(), true);
  assert(inner->insert(innerX) == nullptr);
  assert(inner->lookup("x") == innerX);
  assert(inner->lookupLocal("x") == innerX);

  // The outer binding is unchanged and still visible from the outer scope.
  assert(outer.lookup("x") == x);
  // Parent-walking lookup from a grandchild finds the outermost binding when
  // the middle scope has no binding.
  auto *grandchild = inner->createChild();
  assert(grandchild->lookup("x") == innerX); // resolved via inner scope
  assert(grandchild->lookupLocal("x") == nullptr);
}

void test_scope_unresolved_lookup() {
  Scope root;
  assert(root.lookup("missing") == nullptr);
  assert(root.lookupLocal("missing") == nullptr);
}

void test_symbol_kinds() {
  auto v = std::make_shared<VarSymbol>("v", Universe::Bool(), false, SourceLocation{}, true);
  assert(v->kind() == SymbolKind::Variable);
  assert(v->isExported());
  assert(!v->isMutable());
  assert(v->type() == Universe::Bool());

  auto f = std::make_shared<FuncSymbol>("f", Universe::Func());
  assert(f->kind() == SymbolKind::Function);
  assert(f->name() == "f");

  auto c = std::make_shared<ClassSymbol>("Circle");
  assert(c->kind() == SymbolKind::Class);
  assert(c->name() == "Circle");
}

void test_environment_guard_restores_state() {
  Environment env;
  env.loopDepth = 2;
  env.allowBreak = true;

  {
    EnvironmentGuard guard(env, Environment{});
    assert(env.loopDepth == 0);
    assert(!env.allowBreak);
    env.allowContinue = true;
  }
  // Prior state restored after the guard leaves scope.
  assert(env.loopDepth == 2);
  assert(env.allowBreak);
  assert(!env.allowContinue);
}

void test_cycle_detector_detects_inheritance_cycle() {
  CycleDetector detector;
  // A -> B -> C -> A
  assert(detector.advance("A", {"B"}).empty());
  assert(detector.advance("B", {"C"}).empty());
  const auto cycle = detector.advance("C", {"A"});
  assert(cycle == "A"); // C depends on A while A is Grey
  assert(detector.isResolving("A"));
}

void test_cycle_detector_accepts_acyclic_dependencies() {
  CycleDetector detector;
  assert(detector.advance("A", {"B", "C"}).empty());
  detector.finish("A");
  assert(!detector.isResolving("A"));

  assert(detector.advance("B", {}).empty());
  detector.finish("B");
  assert(detector.advance("C", {}).empty());
  detector.finish("C");
}

} // namespace

int main() {
  test_scope_insert_and_local_lookup();
  test_scope_shadowing();
  test_scope_unresolved_lookup();
  test_symbol_kinds();
  test_environment_guard_restores_state();
  test_cycle_detector_detects_inheritance_cycle();
  test_cycle_detector_accepts_acyclic_dependencies();
  return 0;
}
