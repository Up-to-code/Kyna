#include "cli_commands.hpp"
#include "kyna/diagnostics/diagnostic_renderer.hpp"
#include <fstream>
#include <sstream>

namespace kyna::cli {

std::string readInput(const std::string &path, std::istream &standardInput, std::string &error) {
  std::ostringstream contents;
  if (path == "-") {
    contents << standardInput.rdbuf();
    return contents.str();
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open '" + path + "'";
    return {};
  }
  contents << file.rdbuf();
  return contents.str();
}

int renderResult(const LanguageResult &result, const Options &options, LanguageSession &session,
                 std::ostream &errors) {
  if (!result.diagnostics.empty()) {
    errors << (options.jsonDiagnostics
                   ? renderJsonDiagnostics(result.diagnostics, session.sourceManager())
                   : options.richTerminal
                         ? renderRichDiagnostics(result.diagnostics, session.sourceManager())
                         : renderCompilerDiagnostics(result.diagnostics, session.sourceManager(),
                                                     {options.color}))
           << '\n';
  }
  if (result.ok())
    return 0;
  for (const auto &diagnostic : result.diagnostics)
    if (diagnostic.code == "K4000" || diagnostic.code == "K4001")
      return 2;
  return 1;
}

int dispatch(const Options &options, std::istream &input, std::ostream &output,
             std::ostream &errors) {
  if (options.command == Command::Invalid) {
    errors << "kyna: " << options.error << "\nTry 'kyna --help'.\n";
    return 2;
  }
  if (options.command == Command::Help) {
    output << "Kyna 1.0.0 language tools\n\n"
              "Usage:\n"
              "  kyna run <file|-> [options]\n"
              "  kyna check <file|-> [options]\n"
              "  kyna repl\n"
              "  kyna tokens <file|-> [--format text|json]\n"
              "  kyna ast <file|-> [--format text|json]\n"
              "  kyna hir <file|-> [--format text|json]\n"
              "  kyna mir <file|-> [--format text|json]\n"
              "  kyna bytecode <file|-> [--format text|json]\n"
              "  kyna <file.kyna>\n\n"
              "Options:\n"
              "  --module-path <dir>          Add a module search root (repeatable)\n"
              "  --diagnostic-format <kind>  text or json\n"
              "  --color <policy>            auto, always, or never\n"
              "  --no-color                  Alias for --color never\n";
    return 0;
  }
  if (options.command == Command::Version) {
    output << "Kyna 1.0.0\n";
    return 0;
  }
  if (options.command == Command::Repl)
    return runRepl(options, input, output, errors);
  LanguageSessionOptions sessionOptions;
  sessionOptions.modulePaths = options.modulePaths;
  LanguageSession session(std::move(sessionOptions));
  switch (options.command) {
  case Command::Run:
    return runSourceFile(options, session, input, output, errors);
  case Command::Check:
    return checkSourceFile(options, session, input, output, errors);
  case Command::Tokens:
    return dumpTokens(options, session, input, output, errors);
  case Command::Ast:
    return dumpSyntax(options, session, input, output, errors);
  case Command::Hir:
    return dumpHir(options, session, input, output, errors);
  case Command::Mir:
    return dumpMir(options, session, input, output, errors);
  case Command::Bytecode:
    return dumpBytecode(options, session, input, output, errors);
  default:
    return 2;
  }
}

} // namespace kyna::cli
