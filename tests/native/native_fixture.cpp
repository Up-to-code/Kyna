// A dependency-free C ABI adapter used to validate loading and result ownership.
#include <kyna/language/native_abi.h>
#include <limits>
namespace {
kyna_native_result twice(const kyna_native_value *arguments, size_t count) {
  kyna_native_result result{};
  if (count != 1 || arguments[0].kind != KYNA_INT) {
    result.error_code = "FIXTURE1001"; result.error_message = "expected one integer";
  } else if (arguments[0].integer > std::numeric_limits<int64_t>::max() / 2 ||
             arguments[0].integer < std::numeric_limits<int64_t>::min() / 2) {
    result.error_code = "FIXTURE1002"; result.error_message = "integer overflow";
  } else { result.value.kind = KYNA_INT; result.value.integer = arguments[0].integer * 2; }
  return result;
}
int releases = 0;
struct ArrayResult { kyna_native_value values[2]{}; };
void releaseArray(void *context) { delete static_cast<ArrayResult *>(context); ++releases; }
kyna_native_result arrayResult(const kyna_native_value *, size_t) {
  kyna_native_result result{};
  try {
    auto *context = new ArrayResult;
    context->values[0].kind = context->values[1].kind = KYNA_INT;
    context->values[0].integer = 21; context->values[1].integer = 42;
    result.value.kind = KYNA_ARRAY; result.value.items = context->values; result.value.size = 2;
    result.context = context; result.release = releaseArray;
  } catch (...) { result.error_code = "FIXTURE1003"; result.error_message = "allocation failed"; }
  return result;
}
kyna_native_result brokenResult(const kyna_native_value *arguments, size_t count) {
  auto result = arrayResult(arguments, count);
  if (!result.error_code) result.value.kind = 999;
  return result;
}
kyna_native_result releaseCount(const kyna_native_value *, size_t) {
  kyna_native_result result{}; result.value.kind = KYNA_INT; result.value.integer = releases; return result;
}
kyna_native_result half(const kyna_native_value *arguments, size_t count) {
  kyna_native_result result{};
  if (count != 1 || arguments[0].kind != KYNA_FLOAT) {
    result.error_code = "FIXTURE1004"; result.error_message = "expected promoted float";
  } else { result.value.kind = KYNA_FLOAT; result.value.number = arguments[0].number / 2; }
  return result;
}
const kyna_native_type integer{KYNA_INT, 0, nullptr};
const kyna_native_type array{KYNA_ARRAY, 0, &integer};
const kyna_native_type floating{KYNA_FLOAT, 0, nullptr};
const kyna_native_function functions[] = {
  {"nativeTwice", &integer, 1, integer, twice},
  {"nativeArray", nullptr, 0, array, arrayResult},
  {"nativeBroken", nullptr, 0, integer, brokenResult},
  {"nativeReleases", nullptr, 0, integer, releaseCount},
  {"nativeHalf", &floating, 1, floating, half}
};
const kyna_native_module module{
#ifdef KYNA_TEST_BAD_ABI
  999,
#else
  KYNA_NATIVE_ABI_VERSION,
#endif
  sizeof(kyna_native_module), "fixture", "1.0.0", functions, 5};
}
extern "C" KYNA_NATIVE_EXPORT const kyna_native_module *kyna_native_module_v1(void) { return &module; }
