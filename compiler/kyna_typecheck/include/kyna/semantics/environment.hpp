#pragma once

#include "kyna/semantics/symbol.hpp"
#include "kyna/types/type.hpp"

#include <cstdint>
#include <utility>

namespace kyna::semantics {

// The mutable checking state an analyzer carries while descending into nested
// functions, classes, and loops. Encapsulating it in a single value lets a
// checker restore state atomically even when traversal aborts early on an
// error, instead of manually unwinding several scalar fields.
struct Environment {
  SymbolPtr currentFunc{nullptr};
  SymbolPtr currentClass{nullptr};
  types::TypePtr expectedReturn{nullptr};
  std::uint32_t loopDepth{0};
  std::uint32_t switchDepth{0};
  bool allowBreak{false};
  bool allowContinue{false};
};

// RAII guard that swaps an `Environment` slot to a new value and restores the
// previous one on scope exit. Safe against early returns and exceptions.
class EnvironmentGuard {
public:
  EnvironmentGuard(Environment &slot, Environment next)
      : slot_(slot), previous_(std::move(slot)) {
    slot_ = std::move(next);
  }

  EnvironmentGuard(const EnvironmentGuard &) = delete;
  EnvironmentGuard &operator=(const EnvironmentGuard &) = delete;

  ~EnvironmentGuard() { slot_ = std::move(previous_); }

private:
  Environment &slot_;
  Environment previous_;
};

} // namespace kyna::semantics
