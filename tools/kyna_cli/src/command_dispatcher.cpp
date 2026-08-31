#include "cli_commands.hpp"
#include "kyna/diagnostics/diagnostic_renderer.hpp"
#include <algorithm>
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
  for (const auto &diagnostic : result.diagnostics)
    if (diagnostic.code == "KHTTP0130")
      return 130;
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
    errors << "ky: " << options.error << "\nTry 'ky --help'.\n";
    return 2;
  }
  if (options.command == Command::Help) {
    output << "Kyna 1.0.0 developer platform\n\n"
              "Usage:\n"
              "  ky new [name] [--template minimal|backend]  Create a project or open the wizard\n"
              "  ky init [path] [--template minimal|backend]\n"
              "  ky generate route <name>    Add/register a route; supports --method and --path\n"
              "  ky run [entry]              Run a file or project entry\n"
              "  ky check [entry]            Check without executing\n"
              "  ky fmt [paths...] [--check] Format files or stdin (-)\n"
              "  ky dev | serve              Develop or serve a backend\n"
              "  ky add | remove | install   Manage Git/path dependencies\n"
              "  ky doctor                   Diagnose the environment\n"
              "  ky self update|uninstall    Manage this installation\n"
              "  ky repl|tokens|ast|hir|mir|bytecode|inspect\n"
              "  ky <file.kyna>\n\n"
              "Options:\n"
              "  --module-path <dir>          Add a module search root (repeatable)\n"
              "  --diagnostic-format <kind>  text or json\n"
              "  --color <policy>            auto, always, or never\n"
              "  --no-color                  Alias for --color never\n"
              "  --progress                  Show a TTY-only progress animation\n"
              "  --no-interactive            Disable prompts and animation\n"
              "  --quiet                     Suppress non-essential output\n"
              "  --json                      Emit machine-readable output\n"
              "  --heap-stats                Print garbage-collector statistics after run\n\n"
              "The legacy `kyna` executable is a supported 1.x alias.\n";
    return 0;
  }
  if (options.command == Command::Version) {
    output << "ky 1.0.0 (Kyna 1.0.0)\n";
    return 0;
  }
  if (options.command == Command::Repl)
    return runRepl(options, input, output, errors);
  if (options.command == Command::Inspect)
    return inspectSourceBytes(options, input, output, errors);
  if (options.command == Command::New || options.command == Command::Init ||
      options.command == Command::Generate ||
      options.command == Command::Format || options.command == Command::Dev ||
      options.command == Command::Serve || options.command == Command::Add ||
      options.command == Command::Remove || options.command == Command::Install ||
      options.command == Command::Doctor || options.command == Command::SelfUpdate ||
      options.command == Command::SelfUninstall)
    return runProjectCommand(options, input, output, errors);
  Options effective = options;
  if ((effective.command == Command::Run || effective.command == Command::Check) &&
      effective.input.empty()) {
    const auto root = discoverProject();
    if (root.empty()) {
      errors << "ky: no input provided and no kyna.toml found\n";
      return 2;
    }
    std::string error;
    effective.input = projectEntry(root, error).string();
    if (!error.empty()) {
      errors << "ky: " << error << '\n';
      return 2;
    }
    effective.modulePaths.push_back(root);
  }
  if (effective.command == Command::Run && effective.input != "-") {
    const auto root = discoverProject(effective.input);
    if (!root.empty()) {
      std::string error;
      if (!applyProjectServerEnvironment(root, error)) {
        errors << "ky run: " << error << '\n';
        return 2;
      }
      if (std::find(effective.modulePaths.begin(), effective.modulePaths.end(), root) ==
          effective.modulePaths.end())
        effective.modulePaths.push_back(root);
    }
  }
  LanguageSessionOptions sessionOptions;
  sessionOptions.modulePaths = effective.modulePaths;
  LanguageSession session(std::move(sessionOptions));
  switch (effective.command) {
  case Command::Run:
    return runSourceFile(effective, session, input, output, errors);
  case Command::Check:
    return checkSourceFile(effective, session, input, output, errors);
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
  case Command::Inspect:
    return inspectSourceBytes(options, input, output, errors);
  default:
    return 2;
  }
}

} // namespace kyna::cli
