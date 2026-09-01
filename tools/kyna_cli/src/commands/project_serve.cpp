#include "project_internals.hpp"
#include <cstdlib>

namespace kyna::cli {
namespace {
bool setServerEnvironment(const fs::path &root, const Options &options, std::string &error) {
  auto manifest = loadManifest(root, error);
  if (!error.empty())
    return false;
  auto host = options.host.empty()
                  ? manifest["server"]["host"].value_or(std::string("127.0.0.1"))
                  : options.host;
  auto port = options.port > 0
                  ? options.port
                  : static_cast<int>(manifest["server"]["port"].value_or(std::int64_t{3000}));
#if defined(_WIN32)
  _putenv_s("KYNA_SERVER_HOST", host.c_str());
  _putenv_s("KYNA_SERVER_PORT", std::to_string(port).c_str());
#else
  setenv("KYNA_SERVER_HOST", host.c_str(), 1);
  setenv("KYNA_SERVER_PORT", std::to_string(port).c_str(), 1);
#endif
  return true;
}
} // namespace

int serveProject(const Options &options, std::istream &input, std::ostream &output,
                 std::ostream &errors) {
  const auto root = discoverProject();
  if (root.empty()) {
    errors << "ky serve: no kyna.toml found\n";
    return 2;
  }
  std::string error;
  auto manifest = loadManifest(root, error);
  if (!error.empty()) {
    errors << "ky serve: " << error << '\n';
    return 2;
  }
  auto host = options.host.empty()
                  ? manifest["server"]["host"].value_or(std::string("127.0.0.1"))
                  : options.host;
  auto port = options.port > 0
                  ? options.port
                  : static_cast<int>(manifest["server"]["port"].value_or(std::int64_t{3000}));
  if (!setServerEnvironment(root, options, error)) {
    errors << "ky serve: " << error << '\n';
    return 2;
  }
  Options run = options;
  run.command = Command::Run;
  run.input = projectEntry(root, error).string();
  run.modulePaths.push_back(root);
  if (!error.empty()) {
    errors << "ky serve: " << error << '\n';
    return 2;
  }
  if (!options.quiet)
    errors << "Kyna server listening on http://" << host << ':' << port
           << "  (Ctrl-C to stop)\n";
  LanguageSessionOptions sessionOptions;
  sessionOptions.modulePaths = run.modulePaths;
  LanguageSession session(std::move(sessionOptions));
  return runSourceFile(run, session, input, output, errors);
}

} // namespace kyna::cli
