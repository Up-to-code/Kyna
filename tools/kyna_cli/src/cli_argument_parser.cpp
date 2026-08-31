#include "cli_commands.hpp"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <string_view>

namespace kyna::cli {
namespace {
constexpr std::array<std::string_view, 21> commandNames{
    "run", "check", "repl", "tokens", "ast", "hir", "mir", "bytecode", "inspect",
    "new", "init", "generate", "g", "fmt", "dev", "serve", "add", "remove", "install",
    "doctor", "self"};
bool isCommand(std::string_view value) {
  return std::find(commandNames.begin(), commandNames.end(), value) != commandNames.end();
}
std::vector<std::string> normalizedArguments(int argc, char **argv) {
  std::vector<std::string> result{argv[0]};
  if (argc > 1 && std::string_view(argv[1])[0] != '-' && !isCommand(argv[1]))
    result.emplace_back("run");
  for (int index = 1; index < argc; ++index) result.emplace_back(argv[index]);
  return result;
}
} // namespace

Options parseArguments(int argc, char **argv) {
  Options options;
  CLI::App app{"Kyna programming language tools"};
  app.set_help_flag();
  app.require_subcommand(0, 1);
  bool help = false, version = false, noColor = false, json = false;
  std::string colorMode{"auto"}, diagnosticFormat{"text"}, outputFormat{"text"};
  app.add_flag("-h,--help", help, "Show command help");
  app.add_flag("-V,--version", version, "Show the Kyna version");
  app.add_option("--module-path", options.modulePaths, "Add a module search directory");
  app.add_option("--diagnostic-format", diagnosticFormat)->check(CLI::IsMember({"text", "json"}));
  app.add_option("--format", outputFormat)->check(CLI::IsMember({"text", "json"}));
  app.add_flag("--json", json, "Emit machine-readable JSON");
  app.add_option("--color", colorMode)->check(CLI::IsMember({"auto", "always", "never"}));
  app.add_flag("--no-color", noColor);
  app.add_flag("--progress", options.progress, "Show TTY progress animation");
  app.add_flag("--heap-stats", options.heapStats);
  app.add_flag("--no-interactive", options.noInteractive);
  app.add_flag("-q,--quiet", options.quiet);
  app.add_option("--source-name", options.sourceName);

  const auto addSource = [&](const char *name, const char *description) {
    auto *command = app.add_subcommand(name, description);
    command->add_option("input", options.input, "Source file or '-'");
    command->fallthrough();
    return command;
  };
  auto *run = addSource("run", "Compile and execute a program or project");
  auto *check = addSource("check", "Check a program or project");
  auto *tokens = addSource("tokens", "Print tokens");
  auto *ast = addSource("ast", "Print syntax tree");
  auto *hir = addSource("hir", "Print HIR");
  auto *mir = addSource("mir", "Print MIR");
  auto *bytecode = addSource("bytecode", "Print bytecode");
  auto *inspect = addSource("inspect", "Inspect source bytes");
  auto *repl = app.add_subcommand("repl", "Start the REPL"); repl->fallthrough();

  auto *newProject = app.add_subcommand("new", "Create a Kyna project");
  newProject->add_option("name", options.input,
                         "Project directory (opens a wizard when omitted)");
  newProject->add_option("-t,--template", options.templateName)->check(CLI::IsMember({"minimal", "backend"}));
  newProject->add_flag("--no-git", options.noGit); newProject->add_flag("--force", options.force);
  newProject->fallthrough();
  auto *init = app.add_subcommand("init", "Initialize an empty directory");
  init->add_option("path", options.input, "Directory to initialize (defaults to '.')");
  init->add_option("-t,--template", options.templateName)->check(CLI::IsMember({"minimal", "backend"}));
  init->add_flag("--no-git", options.noGit); init->add_flag("--force", options.force); init->fallthrough();
  auto *generate = app.add_subcommand("generate", "Generate a project module");
  generate->alias("g");
  auto *generateRoute = generate->add_subcommand("route", "Generate and register a backend route");
  generateRoute->add_option("name", options.generatorName)->required();
  generateRoute->add_option("-m,--method", options.generatorMethod)
      ->check(CLI::IsMember({"get", "post", "put", "patch", "delete"}));
  generateRoute->add_option("--path", options.generatorPath, "URL path (defaults to /<name>)");
  generateRoute->fallthrough();
  generate->fallthrough();
  auto *format = app.add_subcommand("fmt", "Format Kyna files");
  format->add_option("paths", options.inputs); format->add_flag("--check", options.formatCheck); format->fallthrough();
  auto *dev = app.add_subcommand("dev", "Watch, check, and restart a project"); dev->fallthrough();
  auto *serve = app.add_subcommand("serve", "Run the backend HTTP entry point");
  serve->add_option("--host", options.host); serve->add_option("--port", options.port)->check(CLI::Range(1, 65535)); serve->fallthrough();
  auto *add = app.add_subcommand("add", "Add a Git or path dependency");
  add->add_option("name", options.dependencyName)->required(); add->add_option("--git", options.dependencyGit);
  add->add_option("--path", options.dependencyPath); add->add_option("--rev", options.dependencyRevision); add->fallthrough();
  auto *remove = app.add_subcommand("remove", "Remove a dependency");
  remove->add_option("name", options.dependencyName)->required(); remove->fallthrough();
  auto *install = app.add_subcommand("install", "Resolve dependencies");
  install->add_flag("--locked", options.locked); install->fallthrough();
  auto *doctor = app.add_subcommand("doctor", "Diagnose the environment"); doctor->fallthrough();
  auto *self = app.add_subcommand("self", "Manage the installed CLI");
  auto *selfUpdate = self->add_subcommand("update", "Update the CLI");
  selfUpdate->add_option("--channel", options.channel)->check(CLI::IsMember({"stable", "preview"}));
  selfUpdate->add_option("--version", options.installVersion); selfUpdate->add_option("--prefix", options.prefix);
  auto *selfUninstall = self->add_subcommand("uninstall", "Remove the CLI"); selfUninstall->add_option("--prefix", options.prefix);

  auto storage = normalizedArguments(argc, argv);
  std::vector<char *> arguments; for (auto &argument : storage) arguments.push_back(argument.data());
  try { app.parse(static_cast<int>(arguments.size()), arguments.data()); }
  catch (const CLI::ParseError &error) { options.command = Command::Invalid; options.error = error.what(); return options; }

  if (help) options.command = Command::Help; else if (version) options.command = Command::Version;
  else if (*run) options.command = Command::Run; else if (*check) options.command = Command::Check;
  else if (*tokens) options.command = Command::Tokens; else if (*ast) options.command = Command::Ast;
  else if (*hir) options.command = Command::Hir; else if (*mir) options.command = Command::Mir;
  else if (*bytecode) options.command = Command::Bytecode; else if (*inspect) options.command = Command::Inspect;
  else if (*repl || argc == 1) options.command = Command::Repl; else if (*newProject) options.command = Command::New;
  else if (*init) options.command = Command::Init;
  else if (*generateRoute) { options.command = Command::Generate; options.generatorKind = "route"; }
  else if (*format) options.command = Command::Format;
  else if (*dev) options.command = Command::Dev; else if (*serve) options.command = Command::Serve;
  else if (*add) options.command = Command::Add; else if (*remove) options.command = Command::Remove;
  else if (*install) options.command = Command::Install; else if (*doctor) options.command = Command::Doctor;
  else if (*selfUpdate) options.command = Command::SelfUpdate; else if (*selfUninstall) options.command = Command::SelfUninstall;
  else { options.command = Command::Invalid; options.error = "a command or source file is required"; }

  // Accept the familiar `ky run dev` spelling while keeping `ky dev` canonical.
  if (options.command == Command::Run && options.input == "dev") {
    options.command = Command::Dev;
    options.input.clear();
  }

  if (noColor || (std::getenv("NO_COLOR") && colorMode == "auto")) colorMode = "never";
  options.color = colorMode != "never"; options.forceColor = colorMode == "always";
  options.jsonDiagnostics = diagnosticFormat == "json" || json; options.jsonOutput = outputFormat == "json" || json;
  const bool inspection = options.command == Command::Tokens || options.command == Command::Ast ||
      options.command == Command::Hir || options.command == Command::Mir || options.command == Command::Bytecode || options.command == Command::Inspect;
  if (inspection && options.input.empty()) { options.command = Command::Invalid; options.error = "command requires a source file or '-'"; }
  if (options.command == Command::Add && (options.dependencyGit.empty() == options.dependencyPath.empty())) {
    options.command = Command::Invalid; options.error = "add requires exactly one of --git or --path";
  }
  return options;
}
} // namespace kyna::cli
