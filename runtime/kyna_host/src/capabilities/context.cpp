#include "kyna/execution/runtime_capabilities.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace kyna::detail {

// The inert root context: never cancelled and has no deadline.
class BackgroundContext final : public Context {
public:
  bool isCancelled() const override { return false; }
  std::optional<std::chrono::system_clock::time_point> deadline() const override {
    return std::nullopt;
  }
  void cancel() override {}
};

// A derived context with optional parent linkage, a deadline, and cancellable
// state. Cancelling a child propagates up to its parent.
class MutableContext final : public Context {
public:
  explicit MutableContext(std::shared_ptr<Context> parent,
                          std::optional<std::chrono::milliseconds> timeout)
      : parent_(std::move(parent)) {
    if (timeout)
      deadline_ = std::chrono::system_clock::now() + *timeout;
  }

  bool isCancelled() const override {
    if (cancelled_.load(std::memory_order_relaxed))
      return true;
    if (parent_ && parent_->isCancelled())
      return true;
    if (deadline_ && std::chrono::system_clock::now() >= *deadline_)
      return true;
    return false;
  }

  std::optional<std::chrono::system_clock::time_point> deadline() const override {
    if (deadline_)
      return deadline_;
    if (parent_)
      return parent_->deadline();
    return std::nullopt;
  }

  void cancel() override {
    cancelled_.store(true, std::memory_order_relaxed);
    if (parent_)
      parent_->cancel();
  }

private:
  std::shared_ptr<Context> parent_;
  std::atomic<bool> cancelled_{false};
  std::optional<std::chrono::system_clock::time_point> deadline_;
};

} // namespace kyna::detail

namespace kyna {

std::shared_ptr<Context> Context::Background() {
  static const auto background = std::make_shared<detail::BackgroundContext>();
  return background;
}

std::pair<std::shared_ptr<Context>, std::function<void()>>
Context::WithTimeout(const std::shared_ptr<Context> &parent, std::chrono::milliseconds timeout) {
  auto context = std::make_shared<detail::MutableContext>(parent, timeout);
  return {context, [context]() { context->cancel(); }};
}

} // namespace kyna
