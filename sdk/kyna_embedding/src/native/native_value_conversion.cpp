// Owns conversion between VM values and the C module ABI.
#include "native_module_private.hpp"
#include <stdexcept>

namespace kyna::native {
namespace {
void bounded(unsigned depth, std::size_t size = 0) {
  if (depth > 64 || size > 10000000) throw std::runtime_error("native value exceeds conversion bounds");
}
}
TypeRef decodeType(const kyna_native_type &type, unsigned depth) {
  bounded(depth);
  static const char *names[] = {"null", "bool", "int", "float", "str", "array"};
  if (type.kind > KYNA_ARRAY || type.nullable > 1) throw std::runtime_error("invalid native type descriptor");
  TypeRef result{names[type.kind], type.nullable != 0, {}, {}};
  if (type.kind == KYNA_ARRAY) {
    if (!type.element) throw std::runtime_error("native array contract requires an element type");
    result.typeArgs.push_back(decodeType(*type.element, depth + 1));
  } else if (type.element) throw std::runtime_error("non-array native type has an element type");
  return result;
}
RuntimeValue decodeValue(const kyna_native_value &value, Heap &heap, unsigned depth) {
  bounded(depth, value.size);
  switch (value.kind) {
  case KYNA_NULL: return Value();
  case KYNA_BOOL:
    if (value.integer != 0 && value.integer != 1) throw std::runtime_error("invalid native boolean");
    return Value(value.integer != 0);
  case KYNA_INT: return Value(value.integer);
  case KYNA_FLOAT: return Value(value.number);
  case KYNA_STRING:
    if (value.size && !value.text) throw std::runtime_error("null native string buffer");
    return Value(std::string(value.text ? value.text : "", value.size));
  case KYNA_ARRAY: {
    if (value.size && !value.items) throw std::runtime_error("null native array buffer");
    Value result(heap.allocateArray());
    auto roots = heap.rootScope(); roots.protect(result);
    auto array = std::get<ArrayPtr>(result.data);
    array->elements.reserve(value.size);
    for (std::size_t index = 0; index < value.size; ++index)
      array->elements.push_back(decodeValue(value.items[index], heap, depth + 1));
    return result;
  }
  default: throw std::runtime_error("unknown native value kind");
  }
}
kyna_native_value InputArena::encode(const RuntimeValue &value, const kyna_native_type *contract, unsigned depth) {
  bounded(depth);
  kyna_native_value result{};
  if (std::holds_alternative<std::nullptr_t>(value.data)) result.kind = KYNA_NULL;
  else if (const auto *boolean = std::get_if<bool>(&value.data)) { result.kind = KYNA_BOOL; result.integer = *boolean; }
  else if (const auto *integer = std::get_if<std::int64_t>(&value.data)) { if (contract && contract->kind == KYNA_FLOAT) { result.kind = KYNA_FLOAT; result.number = static_cast<double>(*integer); }
    else { result.kind = KYNA_INT; result.integer = *integer; } }
  else if (const auto *number = std::get_if<double>(&value.data)) { result.kind = KYNA_FLOAT; result.number = *number; }
  else if (const auto *text = std::get_if<std::string>(&value.data)) { result.kind = KYNA_STRING; result.text = text->data(); result.size = text->size(); }
  else if (const auto *array = std::get_if<ArrayPtr>(&value.data); array && *array) {
    bounded(depth, (*array)->elements.size());
    arrays.emplace_back();
    auto &items = arrays.back();
    items.reserve((*array)->elements.size());
    for (const auto &item : (*array)->elements) items.push_back(encode(item, contract ? contract->element : nullptr, depth + 1));
    result.kind = KYNA_ARRAY; result.items = items.data(); result.size = items.size();
  } else throw std::runtime_error("value cannot cross the native C ABI");
  return result;
}
}
