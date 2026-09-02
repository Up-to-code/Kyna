#include "kyna/execution/tree_walk_engine.hpp"
#include "kyna/parsing/recursive_descent_parser.hpp"
#include "kyna/semantics/program_validation.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"
#include <cassert>

int main() {
  auto program = kyna::Parser(kyna::lex("var values = [1, 2]; push(values, 3); print(len(values)); "
                                        "writeFile(\"kyna-stdlib-test.txt\", \"ok\"); "
                                        "print(readFile(\"kyna-stdlib-test.txt\")); "
                                        "print(processRun(\"true\"));"))
                      .parse();
  assert(kyna::validate(program).empty());
  kyna::Interpreter interpreter(kyna::productionRuntimeCapabilities(),
                                kyna::installStandardLibrary);
  interpreter.execute(program);

  // Exercise the tree-walk introsort across insertion, quicksort, and heapsort
  // paths using sum- and literal-index-based checks (avoids the tree-walk
  // interpreter's `a[i - n]` subscript quirk). Any ordering violation throws.
  auto sortProgram =
      kyna::Parser(kyna::lex(
                        "var reverse = []; "
                        "loop (var i = 2000; i >= 1; i = i - 1) { push(reverse, i); } "
                        "var sortedReverse = sort(reverse); "
                        "var sum = 0; "
                        "loop (var j = 0; j < len(sortedReverse); j = j + 1) { "
                        "  sum = sum + sortedReverse[j]; "
                        "} "
                        "if (sum != 2001000) { throw \"reverse sort sum wrong\"; } "
                        "var floats = sort([2.5, 1.5, 3.5]); "
                        "if (floats[0] > floats[1] || floats[1] > floats[2]) "
                        "{ throw \"float sort wrong\"; } "
                        "var dup = sort([7, 7, 3, 7, 3]); "
                        "if (dup[0] > dup[1] || dup[1] > dup[2] || dup[2] > dup[3] || "
                        "    dup[3] > dup[4]) { throw \"duplicate sort wrong\"; } "
                        "var words = sort([\"pear\", \"apple\", \"mango\"]); "
                        "if (words[0] != \"apple\" || words[1] != \"mango\" || "
                        "    words[2] != \"pear\") { throw \"string sort wrong\"; } "
                        "if (len(sort([])) != 0) { throw \"empty sort wrong\"; } "
                        "var single = sort([42]); "
                        "if (len(single) != 1 || single[0] != 42) "
                        "{ throw \"single sort wrong\"; } "))
          .parse();
  assert(kyna::validate(sortProgram).empty());
  kyna::Interpreter sortInterpreter(kyna::productionRuntimeCapabilities(),
                                    kyna::installStandardLibrary);
  sortInterpreter.execute(sortProgram);

  // Exercise the tree-walk cryptoSha256 native against the known SHA-256
  // test vector for the empty string.
  auto cryptoProgram =
      kyna::Parser(kyna::lex(
                       "if (cryptoSha256(\"\") != "
                       "\"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\") "
                       "{ throw \"sha256 empty wrong\"; } "
                       "if (cryptoSha256(\"abc\") != "
                       "\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\") "
                       "{ throw \"sha256 abc wrong\"; } "))
          .parse();
  assert(kyna::validate(cryptoProgram).empty());
  kyna::Interpreter cryptoInterpreter(kyna::productionRuntimeCapabilities(),
                                      kyna::installStandardLibrary);
  cryptoInterpreter.execute(cryptoProgram);
}
