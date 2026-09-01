#include "project_internals.hpp"
#include <cctype>
#include <set>
#include <sstream>

namespace kyna::cli {
namespace {
bool write(const fs::path &path, std::string_view contents, std::string &error) {
  return projectWrite(path, contents, error);
}
} // namespace

int generateRoute(const Options &options, std::ostream &output, std::ostream &errors) {
  const auto root = discoverProject();
  if (root.empty()) {
    errors << "ky generate: no kyna.toml found\n";
    return 2;
  }
  const auto name = options.generatorName;
  if (name.empty() ||
      name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") !=
          std::string::npos) {
    errors << "ky generate route: use letters, numbers, '-' or '_'\n";
    return 2;
  }
  std::string identifier;
  identifier.reserve(name.size());
  for (const auto character : name)
    identifier +=
        character == '-' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
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
    errors << "ky generate route: this project does not use the generated "
              "routes/index.kyna architecture\n";
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
  if (routeUrl.find_first_of("?# \t\r\n\"") != std::string::npos) {
    errors << "ky generate route: --path cannot contain a query, fragment, whitespace, "
              "or quote\n";
    return 2;
  }
  std::set<std::string> parameterNames;
  std::istringstream segments(routeUrl);
  std::string segment;
  (void)std::getline(segments, segment, '/');
  while (std::getline(segments, segment, '/')) {
    if (segment.empty()) {
      errors << "ky generate route: --path cannot contain empty segments\n";
      return 2;
    }
    if (segment.front() == ':') {
      const auto parameter = segment.substr(1);
      if (parameter.empty() ||
          !(std::isalpha(static_cast<unsigned char>(parameter.front())) ||
            parameter.front() == '_') ||
          parameter.find_first_not_of(
              "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
              std::string::npos) {
        errors << "ky generate route: parameters must look like :userId\n";
        return 2;
      }
      if (!parameterNames.insert(parameter).second) {
        errors << "ky generate route: duplicate parameter :" << parameter << '\n';
        return 2;
      }
    } else if (segment.find_first_not_of(
                   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~") !=
               std::string::npos) {
      errors << "ky generate route: static path segments contain unsupported characters\n";
      return 2;
    }
  }
  const auto registrationPrefix = "app." + options.generatorMethod + "(\"" + routeUrl + "\"";
  if (indexSource.find(registrationPrefix) != std::string::npos) {
    errors << "ky generate route: " << options.generatorMethod << ' ' << routeUrl
           << " is already registered\n";
    return 2;
  }
  auto updatedIndex = indexSource;
  updatedIndex.replace(updatedIndex.find(importMarker), importMarker.size(),
                       importMarker + "import \"./" + name + ".kyna\" as " + alias + ";\n");
  updatedIndex.replace(updatedIndex.find(routeMarker), routeMarker.size(),
                       routeMarker + "    app." + options.generatorMethod + "(\"" + routeUrl +
                           "\", " + alias + ".index);\n");
  const auto routeSource =
      "# ky:route method=" + options.generatorMethod + " path=\"" + routeUrl +
      "\" handler=index\n"
      "export func index(request: any): any {\n"
      "    return http.json({\n"
      "        route: \"" + name + "\",\n"
      "        method: request.method,\n"
      "        path: request.path,\n"
      "        params: request.params,\n"
      "        query: request.query\n"
      "    });\n"
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

} // namespace kyna::cli
