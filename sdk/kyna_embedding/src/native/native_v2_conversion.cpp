// Owns ABI 2 contracts and value/resource/callback conversion.
#include "native_v2_private.hpp"
#include <stdexcept>
namespace kyna::native {
namespace {
void bounds(unsigned depth, std::size_t size = 0) {
  if (depth > 64 || size > 10000000) throw std::runtime_error("native conversion exceeds bounds");
}
void stale(std::string message) { throw KynaError({std::move(message), {}, false, "KNATIVE1006"}); }
}
TypeRef decodeTypeV2(const kyna_native_type_v2 &type, unsigned depth) {
  bounds(depth, type.parameter_count);
  if (type.nullable > 1) throw std::runtime_error("invalid native nullability");
  static const char *names[] = {"null", "bool", "int", "float", "str", "array", "", "func", "void"};
  if (type.kind > KYNA_V2_VOID) throw std::runtime_error("invalid native type");
  TypeRef result{names[type.kind], type.nullable != 0, {}, {}};
  if (type.kind == KYNA_V2_RESOURCE) {
    if (!type.opaque_type || !*type.opaque_type) throw std::runtime_error("resource requires a nominal type");
    result.name = type.opaque_type;
  }
  if (type.kind == KYNA_V2_ARRAY) {
    if (!type.element) throw std::runtime_error("array requires an element type");
    result.typeArgs.push_back(decodeTypeV2(*type.element, depth + 1));
  }
  if (type.kind == KYNA_V2_CALLBACK) {
    if (!type.result || (type.parameter_count && !type.parameters) || type.parameter_count > 1024)
      throw std::runtime_error("callback requires a checked signature");
    for (std::size_t i = 0; i < type.parameter_count; ++i) result.typeArgs.push_back(decodeTypeV2(type.parameters[i], depth + 1));
    result.typeArgs.push_back(decodeTypeV2(*type.result, depth + 1));
  }
  return result;
}
ArenaV2::~ArenaV2() { for (auto handle : retained) owner->release(handle); }
kyna_native_value_v2 ArenaV2::encode(const Value &value, const kyna_native_type_v2 *type, unsigned depth) {
  bounds(depth);
  kyna_native_value_v2 result{};
  if (std::holds_alternative<std::nullptr_t>(value.data)) result.kind = KYNA_V2_NULL;
  else if (auto p = std::get_if<bool>(&value.data)) { result.kind = KYNA_V2_BOOL; result.integer = *p; }
  else if (auto p = std::get_if<std::int64_t>(&value.data)) {
    if (type && type->kind == KYNA_V2_FLOAT) { result.kind = KYNA_V2_FLOAT; result.number = static_cast<double>(*p); }
    else { result.kind = KYNA_V2_INT; result.integer = *p; }
  } else if (auto p = std::get_if<double>(&value.data)) { result.kind = KYNA_V2_FLOAT; result.number = *p; }
  else if (auto p = std::get_if<std::string>(&value.data)) {
    strings.push_back(*p); result.kind = KYNA_V2_STRING; result.text = strings.back().data(); result.size = strings.back().size();
  } else if (auto p = std::get_if<ArrayPtr>(&value.data); p && *p) {
    bounds(depth, (*p)->elements.size()); arrays.emplace_back(); auto &items = arrays.back();
    items.reserve((*p)->elements.size());
    for (const auto &item : (*p)->elements) items.push_back(encode(item, type ? type->element : nullptr, depth + 1));
    result.kind = KYNA_V2_ARRAY; result.items = items.data(); result.size = items.size();
  } else if (auto p = std::get_if<ObjectPtr>(&value.data); p && *p && (*p)->nativeResource) {
    const auto tag = (*p)->nativeResource->typeName();
    auto resource = std::static_pointer_cast<ResourceV2>(owner->resources->get(value, tag));
    if (resource->owner != owner || !owner->module->resource_valid(owner->context, resource->handle, tag.c_str())) stale("stale or foreign resource handle");
    strings.push_back(tag); result.kind = KYNA_V2_RESOURCE; result.handle = resource->handle; result.opaque_type = strings.back().c_str();
  } else if (std::holds_alternative<FunctionPtr>(value.data) || std::holds_alternative<VmClosure *>(value.data) ||
             std::holds_alternative<VmFunctionReference>(value.data) || std::holds_alternative<VmBoundMethod *>(value.data)) {
    result.kind = KYNA_V2_CALLBACK; result.handle = owner->borrow(value, type);
    if (resultRoot.valid()) { owner->retain(result.handle); retained.push_back(result.handle); }
  } else throw std::runtime_error("unsupported native value");
  return result;
}
Value ArenaV2::decode(const kyna_native_value_v2 &value, unsigned depth) {
  bounds(depth, value.size);
  if (owner->closed || owner->active.empty()) stale("native runtime is not active");
  auto &heap = owner->active.back()->heap;
  switch (value.kind) {
  case KYNA_V2_NULL: return {};
  case KYNA_V2_BOOL:
    if (value.integer != 0 && value.integer != 1) throw std::runtime_error("invalid native boolean");
    return Value(value.integer != 0);
  case KYNA_V2_INT: return Value(value.integer);
  case KYNA_V2_FLOAT: return Value(value.number);
  case KYNA_V2_STRING:
    if (value.size && !value.text) throw std::runtime_error("null native string");
    return Value(std::string(value.text ? value.text : "", value.size));
  case KYNA_V2_ARRAY: {
    if (value.size && !value.items) throw std::runtime_error("null native array");
    Value result(heap.allocateArray()); auto roots = heap.rootScope(); roots.protect(result);
    auto array = std::get<ArrayPtr>(result.data); array->elements.reserve(value.size);
    for (std::size_t i = 0; i < value.size; ++i) array->elements.push_back(decode(value.items[i], depth + 1));
    return result;
  }
  case KYNA_V2_RESOURCE: {
    if (!value.opaque_type || !owner->module->resource_valid || !owner->module->resource_destroy ||
        !owner->module->resource_valid(owner->context, value.handle, value.opaque_type)) stale("invalid native resource result");
    auto resource = owner->handles[value.handle].lock();
    if (resource && resource->type != value.opaque_type) stale("resource type changed");
    if (!resource) {
      resource = std::make_shared<ResourceV2>(); resource->owner = owner;
      resource->handle = value.handle; resource->type = value.opaque_type;
      owner->handles[value.handle] = resource;
    }
    const auto tag = resource->type;
    return owner->resources->create(heap, tag, std::move(resource));
  }
  case KYNA_V2_CALLBACK: return owner->lifetime->callbackValue(value.handle);
  default: throw std::runtime_error("invalid native result kind");
  }
}
}
