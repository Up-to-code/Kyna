#include "cli_commands.hpp"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <array>
#include <string_view>

namespace kyna::cli {
namespace {
constexpr std::array<std::string_view, 8> commandNames{"run", "check", "repl", "tokens", "ast",
                                                       "hir", "mir", "bytecode"};

bool isCommand(std::string_view value) {
  return std::find(commandNames.begin(), commandNames.end(), value) != commandNames.end();
}

std::vector<std::string> normalizedArguments(int argc, char **argv) {
  std::vector<std::string> result;
  result.reserve(static_cast<std::size_t>(argc) + 1);
  result.emplace_back(argv[0]);
  if (argc > 1) {
    const std::string_view first = argv[1];
    if (!first.starts_with('-') && !isCommand(first))
      result.emplace_back("run");
  }
  for (int index = 1; index < argc; ++index)
    result.emplace_back(argv[index]);
  return result;
}
} // namespace

Options parseArguments(int argc, char **argv) {
  Options options;
  CLI::App app{"Kyna programming language tools"};
  app.set_help_flag();
  app.require_subcommand(0, 1);

  bool help = false;
  bool version = false;
  bool noColor = false;
  std::string colorMode{"auto"};
  std::string diagnosticFormat{"text"};
  std::string outputFormat{"text"};
  app.add_flag("-h,--help", help, "Show command help");
  app.add_flag("-V,--version", version, "Show the Kyna version");
  app.add_option("--module-path", options.modulePaths, "Add a module search directory");
  app.add_option("--diagnostic-format", diagnosticFormat, "Diagnostic format")
      ->check(CLI::IsMember({"text", "json"}));
  app.add_option("--format", outputFormat, "Inspection output format")
      ->check(CLI::IsMember({"text", "json"}));
  app.add_option("--color", colorMode, "ANSI color policy")
      ->check(CLI::IsMember({"auto", "always", "never"}));
  app.add_flag("--no-color", noColor, "Alias for --color never");
  app.add_option("--source-name", options.sourceName,
                 "Real source path used for stdin module resolution");

  const auto addInputCommand = [&](const char *name, const char *description) {
    auto *command = app.add_subcommand(name, description);
    command->add_option("input", options.input, "Source file or '-'");
    command->fallthrough();
    return command;
  };
  auto *run = addInputCommand("run", "Compile and execute a Kyna program");
  auto *check = addInputCommand("check", "Check a Kyna program without executing it");
  auto *tokens = addInputCommand("tokens", "Print the token stream");
  auto *ast = addInputCommand("ast", "Print the syntax tree");
  auto *hir = addInputCommand("hir", "Print resolved high-level intermediate representation");
  auto *mir = addInputCommand("mir", "Print verified control-flow intermediate representation");
  auto *bytecode = addInputCommand("bytecode", "Validate and disassemble register bytecode");
  auto *repl = app.add_subcommand("repl", "Start the persistent Kyna REPL");
  repl->fallthrough();

  auto storage = normalizedArguments(argc, argv);
  std::vector<char *> arguments;
  arguments.reserve(storage.size());
  for (auto &argument : storage)
    arguments.push_back(argument.data());
  try {
    app.parse(static_cast<int>(arguments.size()), arguments.data());
  } catch (const CLI::ParseError &error) {
    options.command = Command::Invalid;
    options.error = error.what();
    return options;
  }

  if (help) {
    options.command = Command::Help;
    return options;
  }
  if (version) {
    options.command = Command::Version;
    return options;
  }
  if (*run)
    options.command = Command::Run;
  else if (*check)
    options.command = Command::Check;
  else if (*tokens)
    options.command = Command::Tokens;
  else if (*ast)
    options.command = Command::Ast;
  else if (*hir)
    options.command = Command::Hir;
  else if (*mir)
    options.command = Command::Mir;
  else if (*bytecode)
    options.command = Command::Bytecode;
  else if (*repl || argc == 1)
    options.command = Command::Repl;
  else {
    options.command = Command::Invalid;
    options.error = "a command or source file is required";
    return options;
  }

  if (noColor)
    colorMode = "never";
  options.color = colorMode != "never";
  options.forceColor = colorMode == "always";
  options.jsonDiagnostics = diagnosticFormat == "json";
  options.jsonOutput = outputFormat == "json";
  if (options.command != Command::Repl && options.input.empty()) {
    options.command = Command::Invalid;
    options.error = "command requires a source file or '-'";
  }
  return options;
}

} // namespace kyna::cli
