#include "repl_internals.hpp"
#include <toml++/toml.hpp>
#include <array>

namespace kyna::cli {

const std::array<ReplCommandInfo, 11> replCommands{{
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

} // namespace kyna::cli
