#include <kyna/semantics/export_cache.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace kyna::export_cache {
namespace {

constexpr const char *kMagic = "KYC1";

std::string stampLine(const std::filesystem::path &path) {
  std::error_code error;
  const auto time = std::filesystem::last_write_time(path, error);
  if (error)
    return {};
  const auto size = std::filesystem::file_size(path, error);
  if (error)
    return {};
  std::ostringstream line;
  const auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch());
  line << path.generic_string() << '\t' << static_cast<std::uint64_t>(ticks.count()) << '\t'
       << size;
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
    if (!std::getline(in, got) || got != wanted)
      return false;
  }
  std::string extra;
  return !std::getline(in, extra);
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
    out << line << '\n';
  }
}

void invalidate(const std::filesystem::path &cacheFile) {
  std::error_code error;
  std::filesystem::remove(cacheFile, error);
}

} // namespace kyna::export_cache
