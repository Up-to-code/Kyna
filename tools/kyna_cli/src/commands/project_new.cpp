#include "project_internals.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <set>
#include <sstream>

namespace kyna::cli {
namespace {

std::string projectName(const fs::path &path) { return projectNameOf(path); }
bool write(const fs::path &path, std::string_view contents, std::string &error) {
  return projectWrite(path, contents, error);
}
std::string shellQuote(const std::string &value) { return projectShellQuote(value); }

} // namespace

std::optional<std::string> promptProjectPath(const Options &options) {
  if (!options.interactiveTerminal)
    return std::nullopt;
  using namespace ftxui;
  std::string value;
  std::string validation;
  bool accepted = false;
  bool cancelled = false;
  auto screen = ScreenInteractive::TerminalOutput();
  InputOption inputOptions;
  inputOptions.content = &value;
  inputOptions.placeholder = "my-kyna-app";
  inputOptions.on_change = [&] { validation.clear(); };
  inputOptions.on_enter = [&] {
    const auto first = value.find_first_not_of(" \t\r\n");
    const auto last = value.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) {
      validation = "Enter a project directory";
      return;
    }
    value = value.substr(first, last - first + 1);
    const auto filename = fs::path(value).filename().string();
    if (filename.empty() || filename == "." || filename == ".." ||
        filename.find_first_of("<>:\"|?*") != std::string::npos) {
      validation = "Use a valid directory name";
      return;
    }
    accepted = true;
    screen.Exit();
  };
  inputOptions.transform = [&](InputState state) {
    auto element = std::move(state.element);
    if (options.color)
      element |= color(state.focused ? Color::RGB(232, 228, 255)
                                     : Color::RGB(191, 178, 255));
    return element;
  };
  auto input = Input(std::move(inputOptions));
  auto view = Renderer(input, [&] {
    auto title = text(" ◆ ky new ") | bold;
    auto prompt = text(" Project directory  ") | bold;
    if (options.color) {
      title |= color(Color::RGB(109, 74, 255));
      prompt |= color(Color::RGB(151, 106, 255));
    }
    Elements rows{title, text(" Create a ready-to-run Kyna project ") | dim, separator(),
                  hbox({prompt, input->Render() | flex})};
    if (!validation.empty())
      rows.push_back(text(" ! " + validation) | color(Color::RGB(255, 107, 129)));
    rows.push_back(separator());
    rows.push_back(text(" Enter continue · Esc cancel ") | dim);
    return vbox(std::move(rows)) | borderRounded;
  });
  auto component = CatchEvent(view, [&](const Event &event) {
    if (event == Event::Escape) {
      cancelled = true;
      screen.Exit();
      return true;
    }
    return false;
  });
  screen.Loop(component);
  if (cancelled || !accepted)
    return std::string{};
  return value;
}

std::string selectTemplate(const Options &options, std::istream &input, std::ostream &errors) {
  if (!options.templateName.empty())
    return options.templateName;
  if (!options.interactiveTerminal)
    return "minimal";
  (void)input;
  (void)errors;
  using namespace ftxui;
  std::vector<std::string> entries{"minimal  — command-line project",
                                   "backend  — HTTP API and health route"};
  int selected = 0;
  bool cancelled = false;
  auto screen = ScreenInteractive::TerminalOutput();
  auto menu = Menu(&entries, &selected);
  auto view = Renderer(menu, [&] {
    return vbox({text(" Kyna · create project ") | bold | color(Color::RGB(109, 74, 255)),
                 separator(), menu->Render(), separator(),
                 text(" ↑/↓ or j/k · Enter select · Esc cancel ") | dim}) |
           border;
  });
  auto component = CatchEvent(view, [&](const Event &event) {
    if (event == Event::Character('j')) {
      selected = std::min(selected + 1, 1);
      return true;
    }
    if (event == Event::Character('k')) {
      selected = std::max(selected - 1, 0);
      return true;
    }
    if (event == Event::Return) {
      screen.Exit();
      return true;
    }
    if (event == Event::Escape) {
      cancelled = true;
      screen.Exit();
      return true;
    }
    return false;
  });
  screen.Loop(component);
  return cancelled ? std::string{} : (selected == 1 ? "backend" : "minimal");
}

bool scaffoldProject(const fs::path &root, const std::string &kind, const Options &options,
                     std::ostream &output, std::ostream &errors) {
  std::error_code ec;
  if (fs::exists(root, ec) && !fs::is_directory(root, ec)) {
    errors << "ky: target is not a directory\n";
    return false;
  }
  if (fs::exists(root, ec) && !fs::is_empty(root, ec)) {
    if (!options.force) {
      errors << "ky: refusing to overwrite non-project files in '" << root.string() << "'\n";
      return false;
    }
    const std::set<std::string> allowed{".git", ".gitignore", ".vscode", "README.md",
                                        "kyna.toml", "src", "tests", ".env.example"};
    for (const auto &entry : fs::directory_iterator(root))
      if (!allowed.contains(entry.path().filename().string())) {
        errors << "ky: --force cannot replace '" << entry.path().filename().string() << "'\n";
        return false;
      }
  }
  fs::create_directories(root, ec);
  if (ec) {
    errors << "ky: " << ec.message() << '\n';
    return false;
  }
  const auto name = projectName(root);
  std::string error;
  std::ostringstream manifest;
  manifest << "[project]\nname = \"" << name
           << "\"\nversion = \"0.1.0\"\nentry = \"src/main.kyna\"\ntemplate = \"" << kind
           << "\"\n";
  if (kind == "backend")
    manifest << "\n[server]\nhost = \"127.0.0.1\"\nport = 3000\n";
  manifest << "\n[scripts]\ncheck = \"ky check\"\ntest = \"ky check tests\"\n\n[dependencies]\n";
  const std::string minimal =
      "func greet(name: str): str {\n    return \"Hello, \" + name;\n}\n\n"
      "console.log(greet(\"Kyna\"));\n";
  const std::string backend =
      "import \"./app.kyna\" as application;\n\n"
      "set app = application.createApp();\n"
      "app.listen();\n";
  const auto generatedReadme =
      kind == "backend"
          ? "# " + name +
                "\n\nStart with `ky dev` or `ky serve`.\n\n"
                "The Express-style layout separates `src/main.kyna`, `src/app.kyna`, "
                "middleware, and `src/routes/`. Generate another route with "
                "`ky generate route users`.\n"
          : "# " + name + "\n\nRun with `ky run`.\n";
  if (!write(root / "kyna.toml", manifest.str(), error) ||
      !write(root / "src/main.kyna", kind == "backend" ? backend : minimal, error) ||
      !write(root / ".gitignore", ".env\n.kyna/\nbuild/\n", error) ||
      !write(root / "README.md", generatedReadme, error)) {
    errors << "ky: " << error << '\n';
    return false;
  }
  if (kind == "backend") {
    if (!write(root / ".env.example", "PORT=3000\n", error) ||
        !write(root / "src/app.kyna",
               "import \"./middleware/request_logger.kyna\" as requestLogger;\n"
               "import \"./routes/index.kyna\" as routes;\n\n"
               "export func createApp(): any {\n"
               "    # `ky run`, `ky serve`, and `ky dev` apply [server] from kyna.toml.\n"
               "    set app = http.server();\n"
               "    app.use(requestLogger.handle);\n"
               "    routes.register(app);\n"
               "    return app;\n"
               "}\n",
               error) ||
        !write(root / "src/middleware/request_logger.kyna",
               "export func handle(request: any): any {\n"
               "    console.log(request.method, request.path);\n"
               "    return null;\n"
               "}\n",
               error) ||
        !write(root / "src/routes/health.kyna",
               "export func show(request: any): any {\n"
               "    return http.json({ status: \"ok\" });\n"
               "}\n",
               error) ||
        !write(root / "src/routes/home.kyna",
               "export func show(request: any): any {\n"
               "    return http.json({ name: \"" + name + "\", status: \"ready\" });\n"
               "}\n",
               error) ||
        !write(root / "src/routes/index.kyna",
               "# ky:imports\n"
               "import \"./home.kyna\" as homeRoute;\n"
               "import \"./health.kyna\" as healthRoute;\n\n"
               "export func register(app: any): void {\n"
               "    # ky:routes\n"
               "    app.get(\"/\", homeRoute.show);\n"
               "    app.get(\"/health\", healthRoute.show);\n"
               "}\n",
               error) ||
        !write(root / "tests/health.kyna", "# Add backend checks here.\n", error) ||
        !write(root / ".vscode/extensions.json",
               "{\n    \"recommendations\": [\"kyna-lang.kyna-language-support\"]\n}\n",
               error) ||
        !write(root / ".vscode/settings.json",
               "{\n    \"[kyna]\": { \"editor.formatOnSave\": true }\n}\n", error)) {
      errors << "ky: " << error << '\n';
      return false;
    }
  }
  bool gitInitialized = false;
  if (!options.noGit && std::system("git --version > /dev/null 2>&1") == 0) {
    const auto command = "git -C " + shellQuote(root.string()) + " init --quiet";
    (void)std::system(command.c_str());
    gitInitialized = true;
  }
  if (!options.quiet) {
    output << "\n◆ Created " << name << "\n"
           << "  Template  " << kind << '\n'
           << "  Location  " << root.string() << '\n'
           << "  Entry     src/main.kyna\n"
           << "  Git       " << (gitInitialized ? "initialized" : "skipped") << "\n\n"
           << "Next\n"
           << "  cd " << shellQuote(root.string()) << '\n'
           << "  ky " << (kind == "backend" ? "dev" : "run") << '\n';
  }
  return true;
}

} // namespace kyna::cli
