#pragma once
#include <kyna/language/native_abi_v2.h>
#include <kyna/language/native_lifetimes.hpp>
#include <deque>
namespace kyna::native {
TypeRef decodeTypeV2(const kyna_native_type_v2 &, unsigned depth = 0);
struct ModuleV2;
struct ResourceV2 {
  std::shared_ptr<ModuleV2> owner;
  std::uint64_t handle;
  std::string type;
  ~ResourceV2();
};
struct CallV2 {
  ModuleV2 &owner;
  Heap &heap;
  std::vector<std::uint64_t> borrowed;
  CallV2(ModuleV2 &, Heap &);
  ~CallV2();
};
struct ModuleV2 : std::enable_shared_from_this<ModuleV2> {
  std::shared_ptr<void> library;
  const kyna_native_module_v2_descriptor *module{};
  void *context{};
  const std::thread::id thread{std::this_thread::get_id()};
  bool closed{false};
  std::shared_ptr<NativeLifetime> lifetime{std::make_shared<NativeLifetime>()};
  std::shared_ptr<NativeResources> resources{std::make_shared<NativeResources>()};
  std::unordered_map<std::uint64_t, std::weak_ptr<ResourceV2>> handles;
  std::unordered_map<std::uint64_t, std::size_t> references;
  std::vector<CallV2 *> active;
  kyna_native_host_v2 host{};
  ~ModuleV2();
  void close();
  int retain(std::uint64_t);
  int release(std::uint64_t);
  std::uint64_t borrow(const Value &, const kyna_native_type_v2 * = nullptr);
};
struct ArenaV2 {
  std::shared_ptr<ModuleV2> owner;
  std::deque<std::string> strings;
  std::deque<std::vector<kyna_native_value_v2>> arrays;
  Heap::RetainedRoot resultRoot;
  std::vector<std::uint64_t> retained;
  std::string code, message;
  ~ArenaV2();
  kyna_native_value_v2 encode(const Value &, const kyna_native_type_v2 * = nullptr, unsigned depth = 0);
  Value decode(const kyna_native_value_v2 &, unsigned depth = 0);
};
kyna_native_result_v2 invokeCallbackV2(void *, std::uint64_t, const kyna_native_value_v2 *, std::size_t);
}
