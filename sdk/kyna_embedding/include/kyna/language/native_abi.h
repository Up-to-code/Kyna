/* Stable C boundary for synchronous native modules. No C++ objects cross it. */
#ifndef KYNA_NATIVE_ABI_H
#define KYNA_NATIVE_ABI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KYNA_NATIVE_ABI_VERSION 1u
#if defined(_WIN32)
#define KYNA_NATIVE_EXPORT __declspec(dllexport)
#else
#define KYNA_NATIVE_EXPORT __attribute__((visibility("default")))
#endif
/* Arrays are homogeneous according to their declared element contract. */
enum kyna_native_kind { KYNA_NULL, KYNA_BOOL, KYNA_INT, KYNA_FLOAT, KYNA_STRING, KYNA_ARRAY };
typedef struct kyna_native_type {
  uint32_t kind;
  uint32_t nullable;
  const struct kyna_native_type *element;
} kyna_native_type;
typedef struct kyna_native_value {
  uint32_t kind;
  int64_t integer;
  double number;
  const char *text;
  size_t size;
  const struct kyna_native_value *items;
} kyna_native_value;
/* Result buffers live until release(context), called exactly once by the host.
   Input buffers are borrowed only for invoke. Invoke/release must never throw. */
typedef struct kyna_native_result {
  kyna_native_value value;
  const char *error_code;
  const char *error_message;
  void *context;
  void (*release)(void *context);
} kyna_native_result;
typedef struct kyna_native_function {
  const char *name;
  const kyna_native_type *parameters;
  size_t parameter_count;
  kyna_native_type result;
  kyna_native_result (*invoke)(const kyna_native_value *, size_t);
} kyna_native_function;
typedef struct kyna_native_module {
  uint32_t abi_version;
  uint32_t struct_size;
  const char *name;
  const char *version;
  const kyna_native_function *functions;
  size_t function_count;
} kyna_native_module;
/* Export this symbol. Descriptors remain immutable until the library unloads. */
typedef const kyna_native_module *(*kyna_native_module_entry)(void);
KYNA_NATIVE_EXPORT const kyna_native_module *kyna_native_module_v1(void);
#ifdef __cplusplus
}
#endif
#endif
