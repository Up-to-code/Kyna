#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "kyna/execution/runtime_capabilities.hpp"
#include "../host_private.hpp"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace kyna::detail {

class LocalHostInfo final : public HostInfoPort {
public:
  std::string operatingSystem() const override {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#else
    return "unknown";
#endif
  }

  std::string architecture() const override {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
  }

  std::optional<std::string> workingDirectory(std::string &error) const override {
    std::error_code failure;
    auto directory = std::filesystem::current_path(failure);
    if (failure) {
      error = "read current working directory: " + failure.message();
      return std::nullopt;
    }
    return directory.string();
  }

  bool standardOutputIsTerminal() const override {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
  }

  bool supportsColor() const override {
    if (!standardOutputIsTerminal())
      return false;
    const auto *noColor = std::getenv("NO_COLOR");
    if (noColor)
      return false;
    const auto *terminal = std::getenv("TERM");
    return !terminal || std::string_view(terminal) != "dumb";
  }
};

std::shared_ptr<HostInfoPort> makeLocalHostInfo() {
  return std::make_shared<LocalHostInfo>();
}

} // namespace kyna::detail
