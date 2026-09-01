#include "project_internals.hpp"
#include "../formatting/kyna_formatter.hpp"
#include <algorithm>
#include <filesystem>

namespace kyna::cli {
namespace {
bool write(const fs::path &path, std::string_view contents, std::string &error) {
  return projectWrite(path, contents, error);
}
} // namespace

std::vector<fs::path> formatFiles(const std::vector<std::string> &inputs) {
  std::vector<fs::path> result;
  std::vector<std::string> requested = inputs;
  if (requested.empty()) {
    const auto root = discoverProject();
    requested.push_back(root.empty() ? "." : root.string());
  }
  for (const auto &value : requested) {
    if (value == "-") {
      result.emplace_back("-");
      continue;
    }
    const fs::path path(value);
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
      for (const auto &entry :
           fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied))
        if (entry.is_regular_file() && entry.path().extension() == ".kyna" &&
            entry.path().string().find("/.git/") == std::string::npos)
          result.push_back(entry.path());
    } else
      result.push_back(path);
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

int runFormat(const Options &options, std::istream &input, std::ostream &output,
              std::ostream &errors) {
  bool changed = false;
  bool failed = false;
  for (const auto &path : formatFiles(options.inputs)) {
    std::string source, error;
    if (path == "-")
      source = readInput("-", input, error);
    else
      source = readInput(path.string(), input, error);
    if (!error.empty()) {
      errors << "ky fmt: " << error << '\n';
      failed = true;
      continue;
    }
    const auto formatted = formatKyna(source);
    if (!formatted.ok()) {
      errors << "ky fmt: " << formatted.error << '\n';
      failed = true;
      continue;
    }
    if (path == "-") {
      output << formatted.text;
      continue;
    }
    if (formatted.text != source) {
      changed = true;
      if (!options.formatCheck && !write(path, formatted.text, error)) {
        errors << "ky fmt: " << error << '\n';
        failed = true;
      } else if (!options.quiet)
        output << (options.formatCheck ? "would format " : "formatted ") << path.string()
               << '\n';
    }
  }
  if (failed)
    return 2;
  return options.formatCheck && changed ? 1 : 0;
}

} // namespace kyna::cli
