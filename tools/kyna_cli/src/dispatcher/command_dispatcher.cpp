#include "../cli_commands.hpp"
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
  if (!options.metricsFile.empty()) {
    std::ofstream metrics(options.metricsFile, std::ios::binary);
    metrics << "{\"schema\":\"kyna.metrics/v1\",\"executed\":"
            << (result.executed ? "true" : "false") << ",\"phases\":[";
    bool first = true;
    for (const auto &phase : result.metrics) {
      if (!first) metrics << ',';
      first = false;
      metrics << "{\"phase\":\"" << phase.phase << "\",\"nanoseconds\":"
              << phase.nanoseconds << '}';
    }
    metrics << "]}\n";
    metrics.close();
    if (!metrics) {
      errors << "ky: could not write metrics file\n";
      return 2;
    }
  }
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
    LanguageSession session;
    LanguageResult result;
    Diagnostic diagnostic{options.error, {}, false, "KCLI1001"};
    diagnostic.category = "usage";
    diagnostic.help = "Run 'ky --help' for commands and examples.";
    result.diagnostics.push_back(std::move(diagnostic));
    renderResult(result, options, session, errors);
    return 2;
  }
  if (options.command == Command::Help) {
    output << "Kyna 1.0.0\n\n"
              "Usage: ky <command> [options]\n\n"
              "Code\n"
              "  run [entry]        Run a file or project (ky file.ky also works)\n"
              "  check [entry]      Check types without executing\n"
              "  fmt [paths...]     Format source; --check verifies formatting\n"
              "  repl               Open an interactive session\n\n"
              "Projects\n"
              "  new [name]         Create a minimal/backend project or open the wizard\n"
              "  init [path]        Initialize a project\n"
              "  dev | run dev      Watch, check, and restart\n"
              "  serve              Run the backend entry\n"
              "  generate route     Add a route; supports --method and --path\n"
              "  build [entry]      Package an app; --native builds authored CMake bindings\n"
              "  add/remove/install Manage Git/path/native dependencies\n\n"
              "Tools\n"
              "  doctor             Check your installation\n"
              "  self update/uninstall\n"
              "  tokens/ast/hir/mir/bytecode/inspect\n\n"
              "Common options\n"
              "  --module-path <dir>         Add a module search root\n"
              "  --native-library <file>     Load a trusted C ABI module\n"
              "  --diagnostic-format text|json\n"
              "  --color auto|always|never   --no-color disables color\n"
              "  --metrics-file <path>       Write phase timings separately\n"
              "  --heap-stats                Print heap statistics\n"
              "  --progress                  Show terminal progress\n"
              "  --no-interactive --quiet --json\n\n"
              "Examples\n"
              "  ky check src/main.ky\n"
              "  ky run src/main.ky --no-color\n"
              "  ky install --locked\n\n"
              "The kyna executable remains a supported 1.x alias.\n";
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
      options.command == Command::Build || options.command == Command::Doctor || options.command == Command::SelfUpdate ||
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
  auto sessionOptions = makeSessionOptions(effective);
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
