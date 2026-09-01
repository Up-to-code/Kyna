#include <chrono>
#include <thread>

#include "kyna/execution/runtime_capabilities.hpp"
#include "../host_private.hpp"

namespace kyna::detail {

class SystemClock final : public ClockPort {
public:
  void sleep(std::chrono::milliseconds duration) override { std::this_thread::sleep_for(duration); }
};

std::shared_ptr<ClockPort> makeSystemClock() {
  return std::make_shared<SystemClock>();
}

} // namespace kyna::detail
