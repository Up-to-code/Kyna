// Verifies numeric semantics through both tree-walk and the complete VM pipeline.
#include <kyna/execution/tree_walk_engine.hpp>
#include <kyna/execution/bytecode_virtual_machine.hpp>
#include <kyna/hir/syntax_lowering.hpp>
#include <kyna/mir/hir_lowering.hpp>
#include <kyna/bytecode/program_bytecode_compiler.hpp>
#include <kyna/parsing/recursive_descent_parser.hpp>
#include <cassert>

namespace {
kyna::BytecodeExecutionResult vm(const std::string &source) {
  const auto statements = kyna::Parser(kyna::lex(source)).parse();
  auto hir = kyna::lowerSyntaxToHir("math", kyna::SyntaxTree{kyna::ParsedModule{kyna::UnknownSource, "math", statements, {}}});
  assert(hir.ok());
  auto mir = kyna::lowerHirToMir(*hir.program); assert(mir.ok());
  auto bytecode = kyna::compileMirToBytecode(*mir.program); assert(bytecode.ok());
  return kyna::BytecodeVirtualMachine().execute(*bytecode.module);
}
void parity(const std::string &source, const kyna::Value &expected) {
  kyna::Interpreter tree;
  const auto statements = kyna::Parser(kyna::lex(source)).parse();
  const auto *returned = std::get_if<kyna::ReturnStmt>(&statements.back()->node);
  assert(returned);
  tree.execute(std::vector<kyna::StmtPtr>(statements.begin(), statements.end() - 1));
  auto interpreted = tree.evaluate(returned->value);
  auto compiled = vm(source);
  assert(compiled.ok());
  assert(interpreted.equals(expected));
  assert(compiled.value.equals(expected));
  assert(interpreted.typeName() == compiled.value.typeName());
}
}
int main() {
  parity("fn count(seed: int) { var value = seed; fn next(): int { value = value + 1; return value; } return next; } const next = count(0); return next() - next();", kyna::Value(std::int64_t{-1}));
  parity("fn count(seed: int) { var value = seed; fn next(): int { value = value + 1; return value; } return next; } const next = count(0); var ignored = false && (next() > 0); return next();", kyna::Value(std::int64_t{1}));
  parity("return 9 - 4;", kyna::Value(std::int64_t{5}));
  parity("return 9 * 4;", kyna::Value(std::int64_t{36}));
  parity("return 9 / 4;", kyna::Value(2.25));
  parity("return -9 % 4;", kyna::Value(std::int64_t{-1}));
  parity("return 9007199254740993 > 9007199254740992;", kyna::Value(true));
  parity("return false && (1 / 0 > 0);", kyna::Value(false));
  parity("return true || (1 / 0 > 0);", kyna::Value(true));
  for (const auto &source : {"return 9223372036854775807 + 1;", "return 9223372036854775807 * 2;"}) {
    const auto compiled = vm(source);
    assert(!compiled.ok() && compiled.diagnostics.front().code == "KRT2204");
    kyna::Interpreter tree;
    bool failed = false;
    try { tree.execute(kyna::Parser(kyna::lex(source)).parse()); }
    catch (const kyna::KynaError &error) { failed = error.diagnostic.code == "KRT2204"; }
    assert(failed);
  }
}
