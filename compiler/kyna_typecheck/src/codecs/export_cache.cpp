#include <kyna/semantics/export_cache.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace kyna::export_cache {
namespace {

constexpr const char *kMagic = "KYC2-contracts-20261010";

std::string stampLine(const std::filesystem::path &path) {
  std::error_code error;
  std::ifstream source(path, std::ios::binary);
  if (!source) return {};
  std::ostringstream contents;
  contents << source.rdbuf();
  std::ostringstream line;
  // Exact source bytes make same-size, restored-mtime changes observable and
  // avoid treating a non-cryptographic hash collision as a valid cache hit.
  line << std::quoted(path.generic_string()) << ' ' << std::quoted(contents.str());
  return line.str();
}

} // namespace

std::filesystem::path cachePath(const std::filesystem::path &modulePath) {
  auto path = modulePath;
  path += ".kyc";
  return path;
}

bool enabled() {
  const char *flag = std::getenv("KYNA_DISABLE_EXPORT_CACHE");
  return flag == nullptr || flag[0] == '\0';
}

bool stampMatches(const std::vector<std::filesystem::path> &sources,
                  const std::filesystem::path &cacheFile) {
  if (!enabled() || sources.empty())
    return false;
  std::ifstream in(cacheFile);
  if (!in)
    return false;
  std::string magic;
  if (!std::getline(in, magic) || magic != kMagic)
    return false;
  std::vector<std::string> expected;
  expected.reserve(sources.size());
  for (const auto &source : sources) {
    auto line = stampLine(source);
    if (line.empty())
      return false;
    expected.push_back(std::move(line));
  }
  for (const auto &wanted : expected) {
    std::string got;
    if (!(in >> std::quoted(got)) || got != wanted)
      return false;
  }
  std::string extra;
  return true;
}

void writeStamp(const std::vector<std::filesystem::path> &sources,
                const std::filesystem::path &cacheFile) {
  if (!enabled() || sources.empty())
    return;
  std::ofstream out(cacheFile, std::ios::trunc);
  if (!out)
    return;
  out << kMagic << '\n';
  for (const auto &source : sources) {
    auto line = stampLine(source);
    if (line.empty()) {
      out.close();
      invalidate(cacheFile);
      return;
    }
    out << std::quoted(line) << '\n';
  }
}

void invalidate(const std::filesystem::path &cacheFile) {
  std::error_code error;
  std::filesystem::remove(cacheFile, error);
}

namespace {
void writeType(std::ostream &out, const TypeRef &type) {
  out << std::quoted(type.name) << ' ' << type.nullable << ' ' << type.typeArgs.size() << ' ' << type.unionTypes.size() << ' ';
  for (const auto &argument : type.typeArgs) writeType(out, argument);
  for (const auto &arm : type.unionTypes) writeType(out, arm);
}
bool readType(std::istream &in, TypeRef &type, int depth = 0) {
  std::size_t arguments = 0, arms = 0;
  if (depth > 64 || !(in >> std::quoted(type.name) >> type.nullable >> arguments >> arms) || arguments > 1024 || arms > 1024) return false;
  type.typeArgs.resize(arguments); type.unionTypes.resize(arms);
  for (auto &argument : type.typeArgs) if (!readType(in, argument, depth + 1)) return false;
  for (auto &arm : type.unionTypes) if (!readType(in, arm, depth + 1)) return false;
  return true;
}
}

std::optional<std::map<std::string, TypeRef>> readBindings(const std::filesystem::path &cacheFile) {
  std::ifstream in(cacheFile);
  std::string magic;
  if (!std::getline(in, magic) || magic != kMagic) return std::nullopt;
  std::string line;
  while (in >> std::quoted(line)) {
    if (line == "BINDINGS") break;
  }
  if (line != "BINDINGS") return std::nullopt;
  std::size_t count = 0;
  if (!(in >> count) || count > 100000) return std::nullopt;
  std::map<std::string, TypeRef> result;
  for (std::size_t index = 0; index < count; ++index) {
    std::string name; TypeRef type;
    if (!(in >> std::quoted(name)) || !readType(in, type) || !result.emplace(name, std::move(type)).second) return std::nullopt;
  }
  in >> std::ws;
  if (!in.eof()) return std::nullopt;
  return result;
}

void writeBindings(const std::vector<std::filesystem::path> &sources,
                   const std::filesystem::path &cacheFile,
                   const std::map<std::string, TypeRef> &bindings) {
  writeStamp(sources, cacheFile);
  if (!enabled() || sources.empty()) return;
  std::ofstream out(cacheFile, std::ios::app);
  if (!out) return;
  out << std::quoted("BINDINGS") << '\n' << bindings.size() << '\n';
  for (const auto &[name, type] : bindings) {
    out << std::quoted(name) << ' '; writeType(out, type); out << '\n';
  }
}

} // namespace kyna::export_cache
