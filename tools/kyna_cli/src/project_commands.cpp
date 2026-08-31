#include "cli_commands.hpp"
#include "kyna_formatter.hpp"
#include <toml++/toml.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <atomic>
#include <csignal>
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#else
#define NOMINMAX
#include <windows.h>
#endif

namespace kyna::cli {
namespace fs = std::filesystem;
namespace {
std::string shellQuote(const std::string &value) {
#if defined(_WIN32)
  std::string quoted{"\""}; for (char c : value) quoted += c == '"' ? "\\\"" : std::string(1, c); return quoted + "\"";
#else
  std::string quoted{"'"}; for (char c : value) quoted += c == '\'' ? "'\\''" : std::string(1, c); return quoted + "'";
#endif
}
bool write(const fs::path &path, std::string_view contents, std::string &error) {
  std::error_code ec; fs::create_directories(path.parent_path(), ec);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) { error = "cannot write '" + path.string() + "'"; return false; }
  file << contents; return true;
}
std::string projectName(const fs::path &path) {
  auto name = path.lexically_normal().filename().string();
  if (name.empty()) name = "kyna-project";
  for (char &c : name) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) c = '-';
  return name;
}
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
  if (!options.templateName.empty()) return options.templateName;
  if (!options.interactiveTerminal) return "minimal";
  (void)input; (void)errors;
  using namespace ftxui;
  std::vector<std::string> entries{"minimal  — command-line project", "backend  — HTTP API and health route"};
  int selected = 0; bool cancelled = false; auto screen = ScreenInteractive::TerminalOutput();
  auto menu = Menu(&entries, &selected);
  auto view = Renderer(menu, [&] {
    return vbox({text(" Kyna · create project ") | bold | color(Color::RGB(109, 74, 255)), separator(), menu->Render(), separator(), text(" ↑/↓ or j/k · Enter select · Esc cancel ") | dim}) | border;
  });
  auto component = CatchEvent(view, [&](const Event &event) {
    if (event == Event::Character('j')) { selected = std::min(selected + 1, 1); return true; }
    if (event == Event::Character('k')) { selected = std::max(selected - 1, 0); return true; }
    if (event == Event::Return) { screen.Exit(); return true; }
    if (event == Event::Escape) { cancelled = true; screen.Exit(); return true; }
    return false;
  });
  screen.Loop(component); return cancelled ? std::string{} : (selected == 1 ? "backend" : "minimal");
}
bool scaffold(const fs::path &root, const std::string &kind, const Options &options,
              std::ostream &output, std::ostream &errors) {
  std::error_code ec;
  if (fs::exists(root, ec) && !fs::is_directory(root, ec)) { errors << "ky: target is not a directory\n"; return false; }
  if (fs::exists(root, ec) && !fs::is_empty(root, ec)) {
    if (!options.force) { errors << "ky: refusing to overwrite non-project files in '" << root.string() << "'\n"; return false; }
    const std::set<std::string> allowed{".git", ".gitignore", ".vscode", "README.md", "kyna.toml", "src", "tests", ".env.example"};
    for (const auto &entry : fs::directory_iterator(root))
      if (!allowed.contains(entry.path().filename().string())) { errors << "ky: --force cannot replace '" << entry.path().filename().string() << "'\n"; return false; }
  }
  fs::create_directories(root, ec); if (ec) { errors << "ky: " << ec.message() << '\n'; return false; }
  const auto name = projectName(root); std::string error;
  std::ostringstream manifest;
  manifest << "[project]\nname = \"" << name << "\"\nversion = \"0.1.0\"\nentry = \"src/main.kyna\"\ntemplate = \"" << kind << "\"\n";
  if (kind == "backend") manifest << "\n[server]\nhost = \"127.0.0.1\"\nport = 3000\n";
  manifest << "\n[scripts]\ncheck = \"ky check\"\ntest = \"ky check tests\"\n\n[dependencies]\n";
  const std::string minimal = "func greet(name: str): str {\n    return \"Hello, \" + name;\n}\n\nconsole.log(greet(\"Kyna\"));\n";
  const std::string backend =
      "import \"./app.kyna\" as application;\n\n"
      "set app = application.createApp();\n"
      "app.listen();\n";
  const auto generatedReadme = kind == "backend"
      ? "# " + name + "\n\nStart with `ky dev` or `ky serve`.\n\n"
        "The Express-style layout separates `src/main.kyna`, `src/app.kyna`, middleware, "
        "and `src/routes/`. Generate another route with "
        "`ky generate route users`.\n"
      : "# " + name + "\n\nRun with `ky run`.\n";
  if (!write(root / "kyna.toml", manifest.str(), error) ||
      !write(root / "src/main.kyna", kind == "backend" ? backend : minimal, error) ||
      !write(root / ".gitignore", ".env\n.kyna/\nbuild/\n", error) ||
      !write(root / "README.md", generatedReadme, error)) {
    errors << "ky: " << error << '\n'; return false;
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
               "}\n", error) ||
        !write(root / "src/middleware/request_logger.kyna",
               "export func handle(request: any): any {\n"
               "    console.log(request.method, request.path);\n"
               "    return null;\n"
               "}\n", error) ||
        !write(root / "src/routes/health.kyna",
               "export func show(request: any): any {\n"
               "    return http.json({ status: \"ok\" });\n"
               "}\n", error) ||
        !write(root / "src/routes/index.kyna",
               "# ky:imports\n"
               "import \"./health.kyna\" as healthRoute;\n\n"
               "export func register(app: any): void {\n"
               "    # ky:routes\n"
               "    app.get(\"/health\", healthRoute.show);\n"
               "}\n", error) ||
        !write(root / "tests/health.kyna", "# Add backend checks here.\n", error) ||
        !write(root / ".vscode/extensions.json", "{\n    \"recommendations\": [\"kyna-lang.kyna-language-support\"]\n}\n", error) ||
        !write(root / ".vscode/settings.json", "{\n    \"[kyna]\": { \"editor.formatOnSave\": true }\n}\n", error)) {
      errors << "ky: " << error << '\n'; return false;
    }
  }
  bool gitInitialized = false;
  if (!options.noGit && std::system("git --version > /dev/null 2>&1") == 0) {
    const auto command = "git -C " + shellQuote(root.string()) + " init --quiet"; (void)std::system(command.c_str());
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

int generateRoute(const Options &options, std::ostream &output, std::ostream &errors) {
  const auto root = discoverProject();
  if (root.empty()) {
    errors << "ky generate: no kyna.toml found\n";
    return 2;
  }
  const auto name = options.generatorName;
  if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") !=
                          std::string::npos) {
    errors << "ky generate route: use letters, numbers, '-' or '_'\n";
    return 2;
  }
  std::string identifier;
  identifier.reserve(name.size());
  for (const auto character : name)
    identifier += character == '-' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  if (std::isdigit(static_cast<unsigned char>(identifier.front())))
    identifier.insert(identifier.begin(), '_');
  const auto routePath = root / "src/routes" / (name + ".kyna");
  const auto indexPath = root / "src/routes/index.kyna";
  if (fs::exists(routePath)) {
    errors << "ky generate route: " << routePath.string() << " already exists\n";
    return 2;
  }
  std::string error;
  const auto indexSource = readInput(indexPath.string(), std::cin, error);
  if (!error.empty()) {
    errors << "ky generate route: this project does not use the generated routes/index.kyna architecture\n";
    return 2;
  }
  const std::string importMarker = "# ky:imports\n";
  const std::string routeMarker = "    # ky:routes\n";
  if (indexSource.find(importMarker) == std::string::npos ||
      indexSource.find(routeMarker) == std::string::npos) {
    errors << "ky generate route: routes/index.kyna is missing its generation markers\n";
    return 2;
  }
  const auto alias = identifier + "Route";
  const auto routeUrl = options.generatorPath.empty() ? "/" + name : options.generatorPath;
  if (routeUrl.empty() || routeUrl.front() != '/') {
    errors << "ky generate route: --path must begin with '/'\n";
    return 2;
  }
  auto updatedIndex = indexSource;
  updatedIndex.replace(updatedIndex.find(importMarker), importMarker.size(),
                       importMarker + "import \"./" + name + ".kyna\" as " + alias + ";\n");
  updatedIndex.replace(updatedIndex.find(routeMarker), routeMarker.size(),
                       routeMarker + "    app." + options.generatorMethod + "(\"" + routeUrl +
                           "\", " + alias + ".index);\n");
  const auto routeSource =
      "export func index(request: any): any {\n"
      "    return http.json({ route: \"" + name + "\", method: request.method });\n"
      "}\n";
  if (!write(routePath, routeSource, error) || !write(indexPath, updatedIndex, error)) {
    errors << "ky generate route: " << error << '\n';
    return 2;
  }
  if (!options.quiet)
    output << "◆ Generated " << options.generatorMethod << ' ' << routeUrl << "\n"
           << "  Route  " << routePath.string() << "\n"
           << "  Wired  src/routes/index.kyna\n";
  return 0;
}

std::vector<fs::path> formatFiles(const std::vector<std::string> &inputs) {
  std::vector<fs::path> result;
  std::vector<std::string> requested = inputs;
  if (requested.empty()) { const auto root = discoverProject(); requested.push_back(root.empty() ? "." : root.string()); }
  for (const auto &value : requested) {
    if (value == "-") { result.emplace_back("-"); continue; }
    const fs::path path(value); std::error_code ec;
    if (fs::is_directory(path, ec)) {
      for (const auto &entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied))
        if (entry.is_regular_file() && entry.path().extension() == ".kyna" && entry.path().string().find("/.git/") == std::string::npos)
          result.push_back(entry.path());
    } else result.push_back(path);
  }
  std::sort(result.begin(), result.end()); result.erase(std::unique(result.begin(), result.end()), result.end()); return result;
}
int runFormat(const Options &options, std::istream &input, std::ostream &output, std::ostream &errors) {
  bool changed = false; bool failed = false;
  for (const auto &path : formatFiles(options.inputs)) {
    std::string source, error;
    if (path == "-") source = readInput("-", input, error); else source = readInput(path.string(), input, error);
    if (!error.empty()) { errors << "ky fmt: " << error << '\n'; failed = true; continue; }
    const auto formatted = formatKyna(source);
    if (!formatted.ok()) { errors << "ky fmt: " << formatted.error << '\n'; failed = true; continue; }
    if (path == "-") { output << formatted.text; continue; }
    if (formatted.text != source) {
      changed = true;
      if (!options.formatCheck && !write(path, formatted.text, error)) { errors << "ky fmt: " << error << '\n'; failed = true; }
      else if (!options.quiet) output << (options.formatCheck ? "would format " : "formatted ") << path.string() << '\n';
    }
  }
  if (failed) return 2; return options.formatCheck && changed ? 1 : 0;
}

fs::path cacheRoot() {
#if defined(_WIN32)
  if (const char *value = std::getenv("LOCALAPPDATA")) return fs::path(value) / "Kyna/cache";
#elif defined(__APPLE__)
  if (const char *value = std::getenv("HOME")) return fs::path(value) / "Library/Caches/Kyna";
#else
  if (const char *value = std::getenv("XDG_CACHE_HOME")) return fs::path(value) / "kyna";
  if (const char *value = std::getenv("HOME")) return fs::path(value) / ".cache/kyna";
#endif
  return fs::temp_directory_path() / "kyna-cache";
}
toml::table loadManifest(const fs::path &root, std::string &error) {
  try { return toml::parse_file((root / "kyna.toml").string()); }
  catch (const toml::parse_error &e) { error = e.description(); return {}; }
}
bool saveManifest(const fs::path &root, const toml::table &table, std::string &error) { std::ostringstream text; text << table; text << '\n'; return write(root / "kyna.toml", text.str(), error); }

int runDependencies(const Options &options, std::ostream &output, std::ostream &errors) {
  const auto root = discoverProject(); if (root.empty()) { errors << "ky: no kyna.toml found\n"; return 2; }
  std::string error; auto manifest = loadManifest(root, error); if (!error.empty()) { errors << "ky: " << error << '\n'; return 2; }
  auto *deps = manifest["dependencies"].as_table(); if (!deps) { manifest.insert("dependencies", toml::table{}); deps = manifest["dependencies"].as_table(); }
  if (options.command == Command::Add) {
    toml::table dependency;
    if (!options.dependencyGit.empty()) { dependency.insert("git", options.dependencyGit); if (!options.dependencyRevision.empty()) dependency.insert("rev", options.dependencyRevision); }
    else dependency.insert("path", options.dependencyPath);
    deps->insert_or_assign(options.dependencyName, std::move(dependency));
    if (!saveManifest(root, manifest, error)) { errors << "ky: " << error << '\n'; return 2; }
  } else if (options.command == Command::Remove) {
    if (!deps->erase(options.dependencyName)) { errors << "ky: dependency '" << options.dependencyName << "' does not exist\n"; return 2; }
    if (!saveManifest(root, manifest, error)) { errors << "ky: " << error << '\n'; return 2; }
  }
  std::ostringstream lock; lock << "# Generated by ky. Do not edit.\nversion = 1\n\n";
  for (const auto &[name, node] : *deps) {
    const auto *dependency = node.as_table(); if (!dependency) continue;
    lock << "[[package]]\nname = \"" << name.str() << "\"\n";
    if (auto path = (*dependency)["path"].value<std::string>()) {
      const auto resolved = fs::weakly_canonical(root / *path); lock << "source = \"path+" << resolved.generic_string() << "\"\nchecksum = \"local\"\n\n";
    } else if (auto git = (*dependency)["git"].value<std::string>()) {
      const auto requested = (*dependency)["rev"].value_or(std::string("HEAD"));
      const auto cache = cacheRoot() / "git" / std::string(name.str()); fs::create_directories(cache.parent_path());
      if (!fs::exists(cache / ".git")) {
        const auto command = "git clone --quiet --no-checkout " + shellQuote(*git) + " " + shellQuote(cache.string());
        if (std::system(command.c_str()) != 0) { errors << "ky install: failed to clone " << *git << '\n'; return 2; }
      }
      const auto fetch = "git -C " + shellQuote(cache.string()) + " fetch --quiet --tags origin"; (void)std::system(fetch.c_str());
      const auto checkout = "git -C " + shellQuote(cache.string()) + " checkout --quiet --detach " + shellQuote(requested);
      if (std::system(checkout.c_str()) != 0) { errors << "ky install: cannot resolve " << requested << '\n'; return 2; }
      const auto revFile = cache / ".kyna-revision";
      const auto revisionCommand = "git -C " + shellQuote(cache.string()) + " rev-parse HEAD > " + shellQuote(revFile.string());
      if (std::system(revisionCommand.c_str()) != 0) return 2;
      std::string revision = readInput(revFile.string(), std::cin, error); while (!revision.empty() && std::isspace(static_cast<unsigned char>(revision.back()))) revision.pop_back();
      lock << "source = \"git+" << *git << "\"\nrevision = \"" << revision << "\"\nchecksum = \"git-tree:" << revision << "\"\n\n";
    }
  }
  const auto lockPath = root / "kyna.lock"; std::string existing;
  if (fs::exists(lockPath)) existing = readInput(lockPath.string(), std::cin, error);
  if (options.locked && existing != lock.str()) { errors << "ky install --locked: kyna.toml and kyna.lock disagree\n"; return 2; }
  if (!options.locked && !write(lockPath, lock.str(), error)) { errors << "ky: " << error << '\n'; return 2; }
  if (!options.quiet) output << (options.command == Command::Remove ? "Removed dependency" : "Dependencies resolved") << "\n";
  return 0;
}

int doctor(const Options &options, std::ostream &output) {
  const auto root = discoverProject(); const bool pathOk = std::getenv("PATH") != nullptr;
  if (options.jsonOutput) output << "{\"cli\":\"ok\",\"version\":\"1.0.0\",\"manifest\":" << (root.empty() ? "false" : "true") << ",\"path\":" << (pathOk ? "true" : "false") << "}\n";
  else { output << "Kyna doctor\n  CLI: ok (ky 1.0.0; kyna compatibility enabled)\n  PATH: " << (pathOk ? "ok" : "missing") << "\n  Project: " << (root.empty() ? "not found" : root.string()) << "\n  Cache: " << cacheRoot().string() << "\n  VS Code setting: kyna.executable (optional)\n"; }
  return pathOk ? 0 : 2;
}

int serveProject(const Options &options, std::istream &input, std::ostream &output,
                 std::ostream &errors) {
  const auto root = discoverProject(); if (root.empty()) { errors << "ky serve: no kyna.toml found\n"; return 2; }
  std::string error; auto manifest = loadManifest(root, error); if (!error.empty()) { errors << "ky serve: " << error << '\n'; return 2; }
  auto host = options.host.empty() ? manifest["server"]["host"].value_or(std::string("127.0.0.1")) : options.host;
  auto port = options.port > 0 ? options.port : static_cast<int>(manifest["server"]["port"].value_or(std::int64_t{3000}));
#if defined(_WIN32)
  _putenv_s("KYNA_SERVER_HOST", host.c_str()); _putenv_s("KYNA_SERVER_PORT", std::to_string(port).c_str());
#else
  setenv("KYNA_SERVER_HOST", host.c_str(), 1); setenv("KYNA_SERVER_PORT", std::to_string(port).c_str(), 1);
#endif
  Options run = options; run.command = Command::Run; run.input = projectEntry(root, error).string(); run.modulePaths.push_back(root);
  if (!error.empty()) { errors << "ky serve: " << error << '\n'; return 2; }
  if (!options.quiet) errors << "Kyna server listening on http://" << host << ':' << port << "  (Ctrl-C to stop)\n";
  LanguageSessionOptions sessionOptions; sessionOptions.modulePaths = run.modulePaths; LanguageSession session(std::move(sessionOptions));
  return runSourceFile(run, session, input, output, errors);
}

#if !defined(_WIN32)
std::atomic_bool devInterrupted{false};
void interruptDev(int) { devInterrupted = true; }
fs::path currentExecutable() {
#if defined(__APPLE__)
  std::uint32_t size = 0; _NSGetExecutablePath(nullptr, &size); std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) == 0) { path.resize(std::char_traits<char>::length(path.c_str())); return fs::weakly_canonical(path); }
#else
  std::string path(4096, '\0'); const auto count = readlink("/proc/self/exe", path.data(), path.size());
  if (count > 0) { path.resize(static_cast<std::size_t>(count)); return path; }
#endif
  return "ky";
}
std::uint64_t sourceFingerprint(const fs::path &root) {
  std::uint64_t fingerprint = 1469598103934665603ULL; std::error_code ec;
  for (const auto &entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
    if (!entry.is_regular_file(ec)) continue; const auto name = entry.path().filename().string();
    if (entry.path().extension() != ".kyna" && name != "kyna.toml" && name != "kyna.lock") continue;
    const auto stamp = entry.last_write_time(ec).time_since_epoch().count();
    fingerprint ^= static_cast<std::uint64_t>(stamp); fingerprint *= 1099511628211ULL;
    fingerprint ^= std::hash<std::string>{}(entry.path().string()); fingerprint *= 1099511628211ULL;
  }
  return fingerprint;
}
pid_t spawnKy(const fs::path &root, const std::vector<std::string> &arguments, bool quiet) {
  const auto executable = currentExecutable(); const auto child = fork();
  if (child != 0) return child;
  (void)chdir(root.c_str()); std::vector<std::string> storage{executable.string()}; storage.insert(storage.end(), arguments.begin(), arguments.end());
  if (quiet) storage.push_back("--quiet"); storage.push_back("--no-color"); storage.push_back("--no-interactive");
  std::vector<char *> argv; for (auto &value : storage) argv.push_back(value.data()); argv.push_back(nullptr);
  execv(executable.c_str(), argv.data()); _exit(127);
}
bool checkProjectProcess(const fs::path &root) {
  const auto child = spawnKy(root, {"check"}, true); int status = 0;
  return child > 0 && waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
int devProject(const Options &options, std::ostream &errors) {
  const auto root = discoverProject(); if (root.empty()) { errors << "ky dev: no kyna.toml found\n"; return 2; }
  if (!checkProjectProcess(root)) { errors << "ky dev: initial check failed; server was not started\n"; return 1; }
  devInterrupted = false; const auto previous = std::signal(SIGINT, interruptDev);
  auto child = spawnKy(root, {"serve"}, options.quiet); auto fingerprint = sourceFingerprint(root);
  if (!options.quiet) errors << "Kyna dev · watching " << root.string()
                             << "\nSave a source, manifest, or lockfile to check and restart. Ctrl-C stops.\n";
  while (!devInterrupted) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); const auto current = sourceFingerprint(root);
    if (current == fingerprint) continue;
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); fingerprint = sourceFingerprint(root);
    if (!checkProjectProcess(root)) { errors << "Check failed · keeping the last good server running.\n"; continue; }
    if (!options.quiet) errors << "Check passed · restarting server.\n";
    if (child > 0) { kill(child, SIGTERM); (void)waitpid(child, nullptr, 0); }
    child = spawnKy(root, {"serve"}, options.quiet);
  }
  if (child > 0) { kill(child, SIGTERM); (void)waitpid(child, nullptr, 0); }
  std::signal(SIGINT, previous); return 130;
}
#else
std::atomic_bool devInterrupted{false};
BOOL WINAPI interruptDev(DWORD event) {
  if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) { devInterrupted = true; return TRUE; }
  return FALSE;
}
std::uint64_t windowsSourceFingerprint(const fs::path &root) {
  std::uint64_t fingerprint = 1469598103934665603ULL; std::error_code ec;
  for (const auto &entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
    if (!entry.is_regular_file(ec)) continue; const auto name = entry.path().filename().string();
    if (entry.path().extension() != ".kyna" && name != "kyna.toml" && name != "kyna.lock") continue;
    fingerprint ^= static_cast<std::uint64_t>(entry.last_write_time(ec).time_since_epoch().count()); fingerprint *= 1099511628211ULL;
  }
  return fingerprint;
}
PROCESS_INFORMATION spawnWindowsKy(const fs::path &root, std::string arguments, bool quiet) {
  char executable[MAX_PATH]{}; GetModuleFileNameA(nullptr, executable, MAX_PATH);
  std::string command = "\"" + std::string(executable) + "\" " + arguments;
  if (quiet) command += " --quiet"; command += " --no-color --no-interactive";
  STARTUPINFOA startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
  std::vector<char> mutableCommand(command.begin(), command.end()); mutableCommand.push_back('\0');
  CreateProcessA(executable, mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NEW_PROCESS_GROUP,
                 nullptr, root.string().c_str(), &startup, &process);
  return process;
}
bool windowsCheck(const fs::path &root) {
  auto process = spawnWindowsKy(root, "check", true); if (!process.hProcess) return false;
  WaitForSingleObject(process.hProcess, INFINITE); DWORD code = 2; GetExitCodeProcess(process.hProcess, &code);
  CloseHandle(process.hThread); CloseHandle(process.hProcess); return code == 0;
}
void stopWindowsChild(PROCESS_INFORMATION &process) {
  if (!process.hProcess) return;
  if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process.dwProcessId)) TerminateProcess(process.hProcess, 130);
  if (WaitForSingleObject(process.hProcess, 3000) == WAIT_TIMEOUT) TerminateProcess(process.hProcess, 130);
  CloseHandle(process.hThread); CloseHandle(process.hProcess); process = {};
}
int devProject(const Options &options, std::ostream &errors) {
  const auto root = discoverProject(); if (root.empty()) { errors << "ky dev: no kyna.toml found\n"; return 2; }
  if (!windowsCheck(root)) { errors << "ky dev: initial check failed; server was not started\n"; return 1; }
  devInterrupted = false; SetConsoleCtrlHandler(interruptDev, TRUE);
  auto child = spawnWindowsKy(root, "serve", options.quiet); auto fingerprint = windowsSourceFingerprint(root);
  if (!options.quiet) errors << "Kyna dev · watching " << root.string()
                             << "\nSave a source, manifest, or lockfile to check and restart. Ctrl-C stops.\n";
  while (!devInterrupted) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); const auto current = windowsSourceFingerprint(root);
    if (current == fingerprint) continue; std::this_thread::sleep_for(std::chrono::milliseconds(200)); fingerprint = windowsSourceFingerprint(root);
    if (!windowsCheck(root)) { errors << "Check failed · keeping the last good server running.\n"; continue; }
    if (!options.quiet) errors << "Check passed · restarting server.\n"; stopWindowsChild(child); child = spawnWindowsKy(root, "serve", options.quiet);
  }
  stopWindowsChild(child); SetConsoleCtrlHandler(interruptDev, FALSE); return 130;
}
#endif

int selfManage(const Options &options, std::ostream &errors) {
  if (options.command == Command::SelfUpdate) {
#if defined(_WIN32)
    const auto powerShellQuote = [](const std::string &value) {
      std::string quoted{"'"};
      for (const char character : value) quoted += character == '\'' ? "''" : std::string(1, character);
      return quoted + "'";
    };
    std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"$installer = irm 'https://github.com/Up-to-code/Kyna/releases/latest/download/install.ps1'; & ([scriptblock]::Create($installer)) -NonInteractive -Channel " + powerShellQuote(options.channel);
    if (!options.installVersion.empty()) command += " -Version " + powerShellQuote(options.installVersion);
    if (!options.prefix.empty()) command += " -Prefix " + powerShellQuote(options.prefix);
    command += "\"";
#else
    std::string command = "curl -fsSL https://github.com/Up-to-code/Kyna/releases/latest/download/install.sh | sh -s -- --channel " + shellQuote(options.channel);
    if (!options.installVersion.empty()) command += " --version " + shellQuote(options.installVersion);
    if (!options.prefix.empty()) command += " --prefix " + shellQuote(options.prefix);
#endif
    return std::system(command.c_str()) == 0 ? 0 : 2;
  }
  fs::path prefix = options.prefix;
  if (prefix.empty()) {
#if defined(_WIN32)
    if (const char *base = std::getenv("LOCALAPPDATA")) prefix = fs::path(base) / "Kyna";
#else
    if (const char *home = std::getenv("HOME")) prefix = fs::path(home) / ".local";
#endif
  }
  if (prefix.empty()) { errors << "ky self uninstall: cannot determine installation prefix\n"; return 2; }
  const auto manifest = prefix / "share/kyna/install-manifest.txt";
  std::vector<fs::path> installedFiles;
  if (std::ifstream input(manifest); input) {
    std::string line;
    while (std::getline(input, line)) {
      const fs::path relative(line); bool safe = !relative.empty() && !relative.is_absolute();
      for (const auto &component : relative) if (component == "..") safe = false;
      if (safe) installedFiles.push_back(prefix / relative);
    }
  }
  if (installedFiles.empty()) {
    installedFiles = {prefix / "bin/ky", prefix / "bin/kyna", prefix / "bin/ky.previous", prefix / "bin/kyna.previous"};
#if defined(_WIN32)
    installedFiles.insert(installedFiles.end(), {prefix / "bin/ky.exe", prefix / "bin/kyna.exe",
                                                 prefix / "bin/ky.exe.previous", prefix / "bin/kyna.exe.previous"});
#endif
  }
#if defined(_WIN32)
  const auto powerShellLiteral = [](const std::string &value) {
    std::string result{"'"}; for (const char character : value) result += character == '\'' ? "''" : std::string(1, character); return result + "'";
  };
  const auto script = fs::temp_directory_path() / ("kyna-uninstall-" + std::to_string(GetCurrentProcessId()) + ".ps1");
  std::ofstream helper(script, std::ios::binary | std::ios::trunc);
  if (!helper) { errors << "ky self uninstall: cannot create cleanup helper\n"; return 2; }
  helper << "\xEF\xBB\xBF$parent = Get-Process -Id " << GetCurrentProcessId() << " -ErrorAction SilentlyContinue\n"
         << "if ($parent) { $parent.WaitForExit() }\n";
  for (const auto &path : installedFiles)
    helper << "Remove-Item -LiteralPath " << powerShellLiteral(path.string()) << " -Force -ErrorAction SilentlyContinue\n";
  helper << "Remove-Item -LiteralPath " << powerShellLiteral(manifest.string()) << " -Force -ErrorAction SilentlyContinue\n"
         << "Remove-Item -LiteralPath $MyInvocation.MyCommand.Path -Force -ErrorAction SilentlyContinue\n";
  helper.close();
  std::string command = "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + script.string() + "\"";
  std::vector<char> mutableCommand(command.begin(), command.end()); mutableCommand.push_back('\0');
  STARTUPINFOA startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
  if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                      nullptr, nullptr, &startup, &process)) {
    errors << "ky self uninstall: cannot launch cleanup helper\n"; return 2;
  }
  CloseHandle(process.hThread); CloseHandle(process.hProcess);
  errors << "Scheduled removal of Kyna executables from " << (prefix / "bin").string() << "\n"; return 0;
#else
  std::error_code ec;
  for (const auto &path : installedFiles) fs::remove(path, ec);
  fs::remove(manifest, ec);
  errors << "Removed Kyna executables from " << (prefix / "bin").string() << "\n"; return 0;
#endif
}
} // namespace

fs::path discoverProject(const fs::path &start) {
  std::error_code ec; auto current = fs::absolute(start, ec); if (fs::is_regular_file(current, ec)) current = current.parent_path();
  while (!current.empty()) { if (fs::exists(current / "kyna.toml", ec)) return current; const auto parent = current.parent_path(); if (parent == current) break; current = parent; }
  return {};
}
fs::path projectEntry(const fs::path &root, std::string &error) {
  auto manifest = loadManifest(root, error); if (!error.empty()) return {};
  auto entry = manifest["project"]["entry"].value<std::string>(); if (!entry) { error = "kyna.toml is missing project.entry"; return {}; }
  return root / *entry;
}

bool applyProjectServerEnvironment(const fs::path &root, std::string &error) {
  auto manifest = loadManifest(root, error);
  if (!error.empty()) return false;
  const auto host = manifest["server"]["host"].value_or(std::string("127.0.0.1"));
  const auto port = static_cast<int>(
      manifest["server"]["port"].value_or(std::int64_t{3000}));
  if (port < 1 || port > 65535) {
    error = "kyna.toml server.port must be between 1 and 65535";
    return false;
  }
#if defined(_WIN32)
  _putenv_s("KYNA_SERVER_HOST", host.c_str());
  _putenv_s("KYNA_SERVER_PORT", std::to_string(port).c_str());
#else
  setenv("KYNA_SERVER_HOST", host.c_str(), 1);
  setenv("KYNA_SERVER_PORT", std::to_string(port).c_str(), 1);
#endif
  return true;
}

int runProjectCommand(const Options &options, std::istream &input, std::ostream &output, std::ostream &errors) {
  switch (options.command) {
  case Command::New: {
    auto target = options.input;
    if (target.empty()) {
      const auto prompted = promptProjectPath(options);
      if (!prompted) {
        errors << "ky new: project name is required in non-interactive mode\n";
        return 2;
      }
      if (prompted->empty())
        return 130;
      target = *prompted;
    }
    const auto kind = selectTemplate(options, input, errors);
    if (kind.empty()) return 130;
    return scaffold(fs::absolute(target).lexically_normal(), kind, options, output, errors) ? 0 : 2;
  }
  case Command::Init: { const auto kind = selectTemplate(options, input, errors); if (kind.empty()) return 130; return scaffold(fs::absolute(options.input.empty() ? "." : options.input).lexically_normal(), kind, options, output, errors) ? 0 : 2; }
  case Command::Generate: return generateRoute(options, output, errors);
  case Command::Format: return runFormat(options, input, output, errors);
  case Command::Add: case Command::Remove: case Command::Install: return runDependencies(options, output, errors);
  case Command::Doctor: return doctor(options, output);
  case Command::Serve: return serveProject(options, input, output, errors);
  case Command::Dev: return devProject(options, errors);
  case Command::SelfUpdate: case Command::SelfUninstall: return selfManage(options, errors);
  default: errors << "ky: command is not implemented by project dispatcher\n"; return 2;
  }
}
} // namespace kyna::cli
