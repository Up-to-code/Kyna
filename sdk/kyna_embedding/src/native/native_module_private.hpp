#pragma once
#include <kyna/language/native_abi.h>
#include <kyna/language/native_functions.hpp>
#include <deque>
namespace kyna::native {
std::vector<NativeFunction> loadModuleV2(std::shared_ptr<void>, void *entry);
TypeRef decodeType(const kyna_native_type &, unsigned depth = 0);
RuntimeValue decodeValue(const kyna_native_value &, Heap &, unsigned depth = 0);
struct InputArena {
  std::deque<std::vector<kyna_native_value>> arrays;
  kyna_native_value encode(const RuntimeValue &, const kyna_native_type *contract = nullptr, unsigned depth = 0);
};
}
