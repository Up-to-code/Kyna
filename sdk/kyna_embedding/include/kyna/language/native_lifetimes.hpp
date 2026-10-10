#pragma once
#include <kyna/language/native_functions.hpp>
#include <thread>
#include <unordered_map>
namespace kyna {
// Session-owned native state. Retained callbacks can survive individual calls;
// invocation is permitted only while the owning runtime is active on its thread.
class NativeLifetime {
public:
  class Activation {
  public:
    Activation(NativeLifetime &, Heap &, const NativeCallbacks &);
    ~Activation();
    Activation(const Activation &) = delete;
    Activation &operator=(const Activation &) = delete;
  private:
    NativeLifetime &owner;
  };
  NativeLifetime();
  std::uint64_t retainCallback(const Value &, std::optional<TypeRef> contract = std::nullopt);
  void releaseCallback(std::uint64_t);
  Value callbackValue(std::uint64_t) const;
  NativeCallResult invokeCallback(std::uint64_t, std::span<const Value>);
  void close();
private:
  struct Active { Heap *heap; const NativeCallbacks *callbacks; };
  struct Callback { Heap *heap; Heap::RetainedRoot root; std::optional<TypeRef> contract; };
  std::thread::id thread;
  std::vector<Active> active;
  std::unordered_map<std::uint64_t, Callback> callbacks;
  std::uint64_t next{1};
  bool closed{false};
  void requireThread() const;
};

// Opaque resource storage is independent of VM fields. Language objects cannot
// manufacture a lease, and closed/foreign leases are rejected before use.
class NativeResources {
public:
  NativeResources();
  ~NativeResources();
  Value create(Heap &, std::string type, std::shared_ptr<void> resource);
  std::shared_ptr<void> get(const Value &, std::string_view type) const;
  void destroy(const Value &);
  void close();
private:
  struct State;
  struct Lease;
  std::shared_ptr<State> state;
};
}
