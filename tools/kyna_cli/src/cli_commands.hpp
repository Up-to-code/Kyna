#pragma once

#include "kyna/language/language_session.hpp"
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace kyna::cli {

enum class Command {
  Run,
  Check,
  Repl,
  Tokens,
  Ast,
  Hir,
  Mir,
  Bytecode,
  Inspect,
  New,
  Init,
  Generate,
  Format,
  Dev,
  Serve,
  Add,
  Remove,
  Install,
  Doctor,
  SelfUpdate,
  SelfUninstall,
  Help,
  Version,
  Invalid
};

struct Options {
  Command command{Command::Repl};
  std::string input;
  std::vector<std::string> inputs;
  std::string sourceName;
  std::vector<std::filesystem::path> modulePaths;
  bool jsonDiagnostics{false};
  bool jsonOutput{false};
  bool color{true};
  bool forceColor{false};
  bool richTerminal{false};
  bool interactiveTerminal{false};
  bool progress{false};
  bool heapStats{false};
  bool noInteractive{false};
  bool quiet{false};
  bool formatCheck{false};
  bool force{false};
  bool noGit{false};
  bool locked{false};
  std::string templateName;
  std::string generatorKind;
  std::string generatorName;
  std::string generatorMethod{"get"};
  std::string generatorPath;
  std::string dependencyName;
  std::string dependencyGit;
  std::string dependencyPath;
  std::string dependencyRevision;
  std::string host;
  int port{0};
  std::string channel{"stable"};
  std::string installVersion;
  std::string prefix;
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
int inspectSourceBytes(const Options &, std::istream &, std::ostream &, std::ostream &);
int runProjectCommand(const Options &, std::istream &, std::ostream &, std::ostream &);
std::filesystem::path discoverProject(const std::filesystem::path &start =
                                          std::filesystem::current_path());
std::filesystem::path projectEntry(const std::filesystem::path &projectRoot,
                                   std::string &error);
bool applyProjectServerEnvironment(const std::filesystem::path &projectRoot,
                                   std::string &error);
std::string readInput(const std::string &path, std::istream &standardInput, std::string &error);
int renderResult(const LanguageResult &, const Options &, LanguageSession &, std::ostream &);
std::string renderRichDiagnostics(const std::vector<Diagnostic> &, const SourceManager &);

} // namespace kyna::cli
