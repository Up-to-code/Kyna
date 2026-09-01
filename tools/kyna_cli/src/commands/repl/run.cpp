#include "repl_internals.hpp"
#include "kyna/diagnostics/diagnostic_renderer.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include <memory>

namespace kyna::cli {

int runRepl(const Options &options, std::istream &input, std::ostream &output,
            std::ostream &errors) {
  auto session = std::make_unique<LanguageSession>(LanguageSessionOptions{options.modulePaths});
  std::string pending;
  std::string line;
  std::vector<std::string> history;
  const auto project = detectReplProject();
  ReplLineEditor editor(history, options.richTerminal, project);
  output << "Kyna 1.0.0 · interactive playground\n";
  if (options.interactiveTerminal) {
    output << "  "
           << (project.initialized ? "Project: " + project.name + " · " + project.version
                                   : "Workspace: " + project.workspace + " · not initialized")
           << "\n  ↑↓ history  ←→ edit  click cursor  type : for commands  Esc cancel\n";
  } else
    output << ">> ";
  while (true) {
    if (options.interactiveTerminal) {
      auto edited = editor.read(!pending.empty(), output);
      if (!edited)
        break;
      line = std::move(*edited);
    } else if (!std::getline(input, line)) {
      break;
    }
    if (!line.empty() && (history.empty() || history.back() != line))
      history.push_back(line);
    if (line == ":quit" || line == ":q")
      break;
    if (line == ":help" || line == ":commands") {
      showReplHelp(output);
      if (!options.interactiveTerminal)
        output << ">> ";
      continue;
    }
    if (line == ":keys") {
      showReplKeys(output);
      if (!options.interactiveTerminal)
        output << ">> ";
      continue;
    }
    if (line == ":project") {
      showProject(project, output);
      if (!options.interactiveTerminal)
        output << ">> ";
      continue;
    }
    if (line == ":history") {
      for (std::size_t index = 0; index < history.size(); ++index)
        output << "  " << index + 1 << "  " << history[index] << '\n';
      if (!options.interactiveTerminal)
        output << ">> ";
      continue;
    }
    if (line == ":cancel") {
      pending.clear();
      output << "input cancelled\n";
      if (!options.interactiveTerminal)
        output << ">> ";
      continue;
    }
    if (line == ":clear") {
      if (options.interactiveTerminal)
        output << "\033[2J\033[H";
      else
        output << "terminal clear requested\n>> ";
      continue;
    }
    if (pending.empty() && line == ":reset") {
      session = std::make_unique<LanguageSession>(LanguageSessionOptions{options.modulePaths});
      output << "session reset\n";
      if (!options.interactiveTerminal)
        output << ">> ";
      continue;
    }
    if (pending.empty() && line.starts_with(":tokens ")) {
      auto result = session->inspectTokens("<repl>", line.substr(8), options.jsonOutput);
      output << result.output;
      if (!options.interactiveTerminal)
        output << ">> ";
      continue;
    }
    if (pending.empty() && line.starts_with(":ast ")) {
      auto result = session->inspectSyntax("<repl>", line.substr(5), options.jsonOutput);
      output << result.output;
      if (!options.interactiveTerminal)
        output << ">> ";
      continue;
    }

    pending += line + '\n';
    SourceFile probe{UnknownSource, "<repl>", pending};
    auto lexed = tokenize(probe);
    auto parsed = parseModule(probe, std::move(lexed.tokens));
    if (parsed.incomplete) {
      // A file requires semicolons, but the interactive prompt accepts a
      // complete one-line declaration or expression when Enter is pressed.
      // Probe that form before deciding the user is entering a multiline block.
      auto completed = pending;
      if (!completed.empty() && completed.back() == '\n')
        completed.insert(completed.size() - 1, ";");
      else
        completed += ';';
      SourceFile completedProbe{UnknownSource, "<repl>", completed};
      auto completedLexed = tokenize(completedProbe);
      auto completedParsed = parseModule(completedProbe, std::move(completedLexed.tokens));
      if (!completedLexed.diagnostics.empty() || !completedParsed.ok()) {
        if (!options.interactiveTerminal)
          output << ".. ";
        continue;
      }
      pending = std::move(completed);
    }
    auto result = session->runSource("<repl>", pending, true);
    renderResult(result, options, *session, errors);
    pending.clear();
    if (!options.interactiveTerminal)
      output << ">> ";
  }
  if (!pending.empty())
    errors << "incomplete input at end of REPL\n";
  return 0;
}

} // namespace kyna::cli
