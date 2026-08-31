#pragma once

#include "kyna/language/language_session.hpp"
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace kyna::cli {

enum class Command { Run, Check, Repl, Tokens, Ast, Hir, Mir, Bytecode, Help, Version, Invalid };

struct Options {
  Command command{Command::Repl};
  std::string input;
  std::string sourceName;
  std::vector<std::filesystem::path> modulePaths;
  bool jsonDiagnostics{false};
  bool jsonOutput{false};
  bool color{true};
  bool forceColor{false};
  bool richTerminal{false};
  std::string error;
};

Options parseArguments(int argc, char **argv);
int dispatch(const Options &options, std::istream &input, std::ostream &output,
             std::ostream &errors);
int runSourceFile(const Options &, LanguageSession &, std::istream &, std::ostream &,
                  std::ostream &);
int checkSourceFile(const Options &, LanguageSession &, std::istream &, std::ostream &,
                    std::ostream &);
int runRepl(const Options &, std::istream &, std::ostream &, std::ostream &);
int dumpTokens(const Options &, LanguageSession &, std::istream &, std::ostream &, std::ostream &);
int dumpSyntax(const Options &, LanguageSession &, std::istream &, std::ostream &, std::ostream &);
int dumpHir(const Options &, LanguageSession &, std::istream &, std::ostream &, std::ostream &);
int dumpMir(const Options &, LanguageSession &, std::istream &, std::ostream &, std::ostream &);
int dumpBytecode(const Options &, LanguageSession &, std::istream &, std::ostream &,
                 std::ostream &);
std::string readInput(const std::string &path, std::istream &standardInput, std::string &error);
int renderResult(const LanguageResult &, const Options &, LanguageSession &, std::ostream &);
std::string renderRichDiagnostics(const std::vector<Diagnostic> &, const SourceManager &);

} // namespace kyna::cli
