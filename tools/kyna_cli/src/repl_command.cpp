#include "cli_commands.hpp"
#include "kyna/diagnostics/diagnostic_renderer.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <toml++/toml.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <memory>
#include <optional>
#include <thread>

namespace kyna::cli {
namespace {

struct ReplProject {
  std::filesystem::path root;
  std::string workspace;
  std::string name;
  std::string version;
  std::string entry;
  std::string templateName;
  bool initialized{false};
};

struct ReplCommandInfo {
  std::string_view command;
  std::string_view description;
};

static constexpr std::array<ReplCommandInfo, 11> replCommands{{
    {":help", "show the REPL guide"},
    {":commands", "show all available commands"},
    {":keys", "show keyboard and mouse controls"},
    {":project", "show the active project and manifest"},
    {":history", "show submitted source and commands"},
    {":reset", "clear declarations and values"},
    {":cancel", "cancel multiline input"},
    {":clear", "clear the terminal"},
    {":tokens ", "inspect tokens for inline source"},
    {":ast ", "inspect syntax for inline source"},
    {":quit", "exit the REPL"},
}};

ReplProject detectReplProject() {
  ReplProject project;
  std::error_code error;
  const auto current = std::filesystem::current_path(error);
  project.workspace = current.filename().string();
  if (project.workspace.empty())
    project.workspace = "workspace";
  project.root = discoverProject(current);
  if (project.root.empty())
    return project;
  try {
    const auto manifest = toml::parse_file((project.root / "kyna.toml").string());
    project.name = manifest["project"]["name"].value_or(project.root.filename().string());
    project.version = manifest["project"]["version"].value_or(std::string("0.0.0"));
    project.entry = manifest["project"]["entry"].value_or(std::string("src/main.kyna"));
    project.templateName = manifest["project"]["template"].value_or(std::string("custom"));
    project.initialized = true;
  } catch (const toml::parse_error &) {
    project.name = project.root.filename().string();
  }
  return project;
}

void showProject(const ReplProject &project, std::ostream &output) {
  output << "Project context\n";
  if (!project.initialized) {
    output << "  Workspace: " << project.workspace << "\n"
           << "  Status:    not initialized (no kyna.toml)\n"
           << "  Start:     ky init --template minimal\n";
    return;
  }
  output << "  Name:      " << project.name << '\n'
         << "  Version:   " << project.version << '\n'
         << "  Template:  " << project.templateName << '\n'
         << "  Entry:     " << project.entry << '\n'
         << "  Root:      " << project.root.string() << '\n';
}

void showReplHelp(std::ostream &output) {
  output << "Commands\n"
            "  :help                 show this guide\n"
            "  :commands             show this command list\n"
            "  :keys                 list keyboard and mouse controls\n"
            "  :project              show project name, entry, and root\n"
            "  :history              list submitted lines\n"
            "  :reset                clear declarations and values\n"
            "  :cancel               cancel multiline input\n"
            "  :clear                clear the terminal\n"
            "  :tokens <code>        inspect tokens\n"
            "  :ast <code>           inspect syntax\n"
            "  :quit / :q            exit\n";
}

void showReplKeys(std::ostream &output) {
  output << "Editing\n"
            "  Left/Right            move the cursor\n"
            "  Ctrl+Left/Right       move one word\n"
            "  Home/End, Ctrl+A/E    move to start/end\n"
            "  Backspace/Delete      remove characters\n"
            "  Ctrl+W/U/K            delete word/before/after cursor\n"
            "  Multi-line paste      preserve lines and submit as one block\n"
            "History and actions\n"
            "  Up/Down                browse history on a single line\n"
            "  Ctrl+P/N               browse history from any line\n"
            "  Ctrl+R                 search backward in history\n"
            "  Tab                    complete a :command\n"
            "  F1                     show commands\n"
            "  Escape                 cancel the current input\n"
            "  Ctrl+L                 clear the terminal\n"
            "  Ctrl+D                 exit on an empty line\n"
            "Mouse\n"
            "  Left click             place the editing cursor\n";
}

class ReplLineEditor {
public:
  ReplLineEditor(std::vector<std::string> &entries, bool colorEnabled,
                 const ReplProject &projectContext)
      : history(entries), colors(colorEnabled), project(projectContext) {}

  std::optional<std::string> read(bool continuation, std::ostream &output) {
    using namespace ftxui;
    std::string content;
    std::string draft;
    int cursor = 0;
    std::size_t historyIndex = history.size();
    bool accepted = false;
    bool endOfInput = false;
    bool cancelled = false;
    std::atomic_size_t pulse{0};
    std::atomic_bool submitRequested{false};
    std::atomic_bool submissionScheduled{false};
    std::atomic_bool running{true};
    std::atomic_int submitNewlinePosition{-1};
    const auto nowMilliseconds = [] {
      return std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now().time_since_epoch())
          .count();
    };
    std::atomic<long long> lastActivity{nowMilliseconds()};

    auto app = App::FitComponent();
    app.TrackMouse(true);
    InputOption inputOptions;
    inputOptions.content = &content;
    inputOptions.placeholder = continuation ? "continue Kyna code…" : "write Kyna code…";
    // FTXUI inserts a newline before calling on_enter in multiline mode. We
    // intentionally wait for a short quiet period before accepting it. A
    // normal Enter therefore submits promptly, while a terminal paste can
    // deliver all of its queued lines without tearing down and recreating the
    // input component between lines.
    inputOptions.multiline = true;
    inputOptions.cursor_position = &cursor;
    inputOptions.on_change = [&] {
      // Any text arriving after an Enter means that newline belongs to a
      // multi-line paste, rather than being the delimiter used to submit a
      // manually edited line.
      submitNewlinePosition = -1;
      lastActivity = nowMilliseconds();
      ++pulse;
    };
    inputOptions.on_enter = [&] {
      submitNewlinePosition = cursor - 1;
      lastActivity = nowMilliseconds();
      submitRequested = true;
    };
    inputOptions.transform = [&](InputState state) {
      auto element = std::move(state.element);
      if (colors)
        element |= color(state.focused ? Color::RGB(232, 228, 255) : Color::RGB(191, 178, 255));
      if (state.hovered)
        element |= underlined;
      return element;
    };
    auto inputComponent = Input(std::move(inputOptions));

    const auto selectHistory = [&](bool older) {
      if (history.empty())
        return;
      if (historyIndex == history.size())
        draft = content;
      if (older) {
        if (historyIndex > 0)
          --historyIndex;
      } else if (historyIndex < history.size()) {
        ++historyIndex;
      }
      content = historyIndex == history.size() ? draft : history[historyIndex];
      cursor = static_cast<int>(content.size());
      ++pulse;
    };
    const auto completeCommand = [&] {
      if (!content.starts_with(':'))
        return false;
      for (const auto &item : replCommands)
        if (item.command.starts_with(content) && item.command != content) {
          content = item.command;
          cursor = static_cast<int>(content.size());
          ++pulse;
          return true;
        }
      return false;
    };

    auto editor = CatchEvent(inputComponent, [&](const Event &event) {
      const bool hasMultipleLines = content.find('\n') != std::string::npos;
      if (event == Event::CtrlP || (event == Event::ArrowUp && !hasMultipleLines)) {
        selectHistory(true);
        return true;
      }
      if (event == Event::CtrlN || (event == Event::ArrowDown && !hasMultipleLines)) {
        selectHistory(false);
        return true;
      }
      if (event == Event::CtrlR) {
        const auto needle = content;
        for (std::size_t index = historyIndex; index > 0; --index)
          if (history[index - 1].find(needle) != std::string::npos) {
            historyIndex = index - 1;
            content = history[historyIndex];
            cursor = static_cast<int>(content.size());
            ++pulse;
            return true;
          }
        return true;
      }
      if (event == Event::CtrlA) { cursor = 0; ++pulse; return true; }
      if (event == Event::CtrlE) { cursor = static_cast<int>(content.size()); ++pulse; return true; }
      if (event == Event::CtrlU) {
        content.erase(0, static_cast<std::size_t>(cursor)); cursor = 0; ++pulse; return true;
      }
      if (event == Event::CtrlK) {
        content.erase(static_cast<std::size_t>(cursor)); ++pulse; return true;
      }
      if (event == Event::CtrlW) {
        auto begin = static_cast<std::size_t>(cursor);
        while (begin > 0 && std::isspace(static_cast<unsigned char>(content[begin - 1]))) --begin;
        while (begin > 0 && !std::isspace(static_cast<unsigned char>(content[begin - 1]))) --begin;
        content.erase(begin, static_cast<std::size_t>(cursor) - begin);
        cursor = static_cast<int>(begin); ++pulse; return true;
      }
      if (event == Event::Tab)
        return completeCommand();
      if (event == Event::F1) {
        content = ":help"; accepted = true; app.Exit(); return true;
      }
      if (event == Event::CtrlL) {
        content = ":clear"; accepted = true; app.Exit(); return true;
      }
      if (event == Event::Escape) {
        cancelled = true; app.Exit(); return true;
      }
      if (event == Event::CtrlD && content.empty()) {
        endOfInput = true; app.Exit(); return true;
      }
      if (event == Event::ArrowLeft || event == Event::ArrowRight ||
          event == Event::ArrowLeftCtrl || event == Event::ArrowRightCtrl || event.is_mouse())
        ++pulse;
      return false;
    });

    static constexpr std::array<std::string_view, 4> motion{"◆", "✦", "◇", "✧"};
    static constexpr std::array<std::string_view, 4> trail{"▰▱▱", "▰▰▱", "▰▰▰", "▱▰▰"};
    auto renderer = Renderer(editor, [&] {
      const auto frame = pulse.load() % motion.size();
      auto mark = text(std::string(motion[frame]));
      const auto context = project.initialized
                               ? project.name + " · " + project.templateName + " · v" +
                                     project.version
                               : project.workspace + " · not initialized";
      auto label = text(" Kyna  " + context) | bold;
      auto movement = text(" " + std::string(trail[frame]) + " ") | dim;
      if (colors) {
        static const std::array<Color, 4> palette{
            Color::RGB(109, 74, 255), Color::RGB(151, 106, 255),
            Color::RGB(76, 201, 240), Color::RGB(210, 125, 255)};
        mark |= color(palette[frame]);
        label |= color(Color::RGB(151, 106, 255));
        movement |= color(palette[(frame + 1) % palette.size()]);
      }
      auto header = hbox({mark, label, filler(), movement});
      auto inputMark = text(continuation ? " ··  " : "  ›  ") | bold;
      if (colors)
        inputMark |= color(Color::RGB(151, 106, 255));
      Elements document{header, separator(),
                        hbox({inputMark, inputComponent->Render() | flex})};
      if (content.starts_with(':') && content.find_first_of(" \n") == std::string::npos) {
        Elements matches;
        for (const auto &item : replCommands) {
          if (!item.command.starts_with(content))
            continue;
          auto command = text("  " + std::string(item.command)) | bold;
          auto description = text("  " + std::string(item.description)) | dim;
          if (colors)
            command |= color(Color::RGB(151, 106, 255));
          matches.push_back(hbox({command, description}));
        }
        if (!matches.empty()) {
          document.push_back(separator());
          document.push_back(text(" Commands · Tab completes ") | dim);
          document.push_back(vbox(std::move(matches)));
        }
      }
      return vbox(std::move(document));
    });

    output.flush();
    std::thread worker([&] {
      auto nextAnimation = nowMilliseconds() + 140;
      while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (!running)
          break;
        const auto now = nowMilliseconds();
        if (submitRequested && !submissionScheduled && now - lastActivity.load() >= 80) {
          submissionScheduled = true;
          app.Post([&] {
            if (nowMilliseconds() - lastActivity.load() >= 80) {
              const auto submitNewline = submitNewlinePosition.load();
              if (submitNewline >= 0 &&
                  static_cast<std::size_t>(submitNewline) < content.size() &&
                  content[static_cast<std::size_t>(submitNewline)] == '\n') {
                content.erase(static_cast<std::size_t>(submitNewline), 1);
                if (cursor > submitNewline)
                  --cursor;
                submitNewlinePosition = -1;
              }
              accepted = true;
              app.Exit();
            } else {
              submissionScheduled = false;
            }
          });
        }
        if (colors && now >= nextAnimation) {
          nextAnimation = now + 140;
          ++pulse;
          app.PostEvent(Event::Custom);
        }
      }
    });
    app.Loop(renderer);
    running = false;
    worker.join();
    if (endOfInput)
      return std::nullopt;
    if (cancelled)
      return std::string(":cancel");
    const auto submitNewline = submitNewlinePosition.load();
    if (accepted && submitNewline >= 0 &&
        static_cast<std::size_t>(submitNewline) < content.size() &&
        content[static_cast<std::size_t>(submitNewline)] == '\n')
      content.erase(static_cast<std::size_t>(submitNewline), 1);
    return accepted ? std::optional<std::string>(std::move(content)) : std::nullopt;
  }

private:
  std::vector<std::string> &history;
  bool colors{false};
  const ReplProject &project;
};

} // namespace

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
  }
  else
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
      if (!options.interactiveTerminal) output << ">> ";
      continue;
    }
    if (line == ":keys") {
      showReplKeys(output);
      if (!options.interactiveTerminal) output << ">> ";
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
      if (!options.interactiveTerminal) output << ">> ";
      continue;
    }
    if (line == ":cancel") {
      pending.clear();
      output << "input cancelled\n";
      if (!options.interactiveTerminal) output << ">> ";
      continue;
    }
    if (line == ":clear") {
      if (options.interactiveTerminal) output << "\033[2J\033[H";
      else output << "terminal clear requested\n>> ";
      continue;
    }
    if (pending.empty() && line == ":reset") {
      session = std::make_unique<LanguageSession>(LanguageSessionOptions{options.modulePaths});
      output << "session reset\n";
      if (!options.interactiveTerminal) output << ">> ";
      continue;
    }
    if (pending.empty() && line.starts_with(":tokens ")) {
      auto result = session->inspectTokens("<repl>", line.substr(8), options.jsonOutput);
      output << result.output;
      if (!options.interactiveTerminal) output << ">> ";
      continue;
    }
    if (pending.empty() && line.starts_with(":ast ")) {
      auto result = session->inspectSyntax("<repl>", line.substr(5), options.jsonOutput);
      output << result.output;
      if (!options.interactiveTerminal) output << ">> ";
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
        if (!options.interactiveTerminal) output << ".. ";
        continue;
      }
      pending = std::move(completed);
    }
    auto result = session->runSource("<repl>", pending, true);
    renderResult(result, options, *session, errors);
    pending.clear();
    if (!options.interactiveTerminal) output << ">> ";
  }
  if (!pending.empty())
    errors << "incomplete input at end of REPL\n";
  return 0;
}

} // namespace kyna::cli
