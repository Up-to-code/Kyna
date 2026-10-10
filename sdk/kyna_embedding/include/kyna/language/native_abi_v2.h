/* ABI 2 adds per-session contexts, nominal resources, and retained callbacks. */
#ifndef KYNA_NATIVE_ABI_V2_H
#define KYNA_NATIVE_ABI_V2_H
#include <kyna/language/native_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KYNA_NATIVE_ABI_V2 2u
enum kyna_native_kind_v2 { KYNA_V2_NULL, KYNA_V2_BOOL, KYNA_V2_INT,
  KYNA_V2_FLOAT, KYNA_V2_STRING, KYNA_V2_ARRAY, KYNA_V2_RESOURCE, KYNA_V2_CALLBACK, KYNA_V2_VOID };
typedef struct kyna_native_type_v2 {
  uint32_t kind, nullable;
  const struct kyna_native_type_v2 *element;
  const char *opaque_type;
  const struct kyna_native_type_v2 *parameters;
  size_t parameter_count;
  const struct kyna_native_type_v2 *result;
} kyna_native_type_v2;
typedef struct kyna_native_value_v2 {
  uint32_t kind;
  int64_t integer;
  double number;
  const char *text;
  size_t size;
  const struct kyna_native_value_v2 *items;
  uint64_t handle;
  const char *opaque_type;
} kyna_native_value_v2;
typedef struct kyna_native_result_v2 {
  kyna_native_value_v2 value;
  const char *error_code, *error_message;
  void *context;
  void (*release)(void *);
} kyna_native_result_v2;
/* Callback IDs passed as arguments are borrowed for that invocation. Retain
   before keeping an ID, release on unsubscribe. Invoke only on the runtime/UI
   thread during an active call (normally gui.run). Result buffers require release. */
typedef struct kyna_native_host_v2 {
  uint32_t abi_version, struct_size;
  void *context;
  int (*retain_callback)(void *, uint64_t);
  int (*release_callback)(void *, uint64_t);
  kyna_native_result_v2 (*invoke_callback)(void *, uint64_t, const kyna_native_value_v2 *, size_t);
} kyna_native_host_v2;
typedef struct kyna_native_function_v2 {
  const char *name;
  const kyna_native_type_v2 *parameters;
  size_t parameter_count;
  kyna_native_type_v2 result;
  kyna_native_result_v2 (*invoke)(void *, const kyna_native_value_v2 *, size_t);
} kyna_native_function_v2;
typedef struct kyna_native_module_v2_descriptor {
  uint32_t abi_version, struct_size;
  const char *name, *version;
  const kyna_native_function_v2 *functions;
  size_t function_count;
  void *(*create)(const kyna_native_host_v2 *);
  void (*close)(void *);
  int (*resource_valid)(void *, uint64_t, const char *);
  void (*resource_destroy)(void *, uint64_t);
} kyna_native_module_v2_descriptor;
typedef const kyna_native_module_v2_descriptor *(*kyna_native_module_v2_entry)(void);
KYNA_NATIVE_EXPORT const kyna_native_module_v2_descriptor *kyna_native_module_v2(void);
#ifdef __cplusplus
}
#endif
#endif
