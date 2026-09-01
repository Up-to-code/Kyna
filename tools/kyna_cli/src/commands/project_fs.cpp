#include "project_internals.hpp"
#include <cctype>
#include <fstream>

namespace kyna::cli {

std::string projectShellQuote(const std::string &value) {
#if defined(_WIN32)
  std::string quoted{"\""};
  for (char c : value)
    quoted += c == '"' ? "\\\"" : std::string(1, c);
  return quoted + "\"";
#else
  std::string quoted{"'"};
  for (char c : value)
    quoted += c == '\'' ? "'\\''" : std::string(1, c);
  return quoted + "'";
#endif
}

bool projectWrite(const fs::path &path, std::string_view contents, std::string &error) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    error = "cannot write '" + path.string() + "'";
    return false;
  }
  file << contents;
  return true;
}

std::string projectNameOf(const fs::path &path) {
  auto name = path.lexically_normal().filename().string();
  if (name.empty())
    name = "kyna-project";
  for (char &c : name)
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'))
      c = '-';
  return name;
}

} // namespace kyna::cli
