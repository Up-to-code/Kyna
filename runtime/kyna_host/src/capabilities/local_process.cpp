#include <cstdlib>
#include <optional>
#include <string>

#include "kyna/execution/runtime_capabilities.hpp"
#include "../host_private.hpp"

namespace kyna::detail {

class LocalProcess final : public ProcessPort {
public:
  int run(const std::string &command) override { return std::system(command.c_str()); }
  std::optional<std::string> environment(const std::string &name) override {
    const auto *value = std::getenv(name.c_str());
    return value ? std::optional<std::string>(value) : std::nullopt;
  }
};

std::shared_ptr<ProcessPort> makeLocalProcess() {
  return std::make_shared<LocalProcess>();
}

} // namespace kyna::detail
