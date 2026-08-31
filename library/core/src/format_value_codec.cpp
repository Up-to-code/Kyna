#include "format_value_codec.hpp"
#include "kyna/execution/runtime_object_model.hpp"
#include <type_traits>
#include <unordered_set>

namespace kyna {
namespace {

Value toRuntime(const FormatValue &value, Heap &heap) {
  return std::visit(
      [&](const auto &item) -> Value {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, FormatValue::Array>) {
          auto *array = heap.allocateArray();
          Value protectedArray(array);
          auto roots = heap.rootScope();
          roots.protect(protectedArray);
          array->elements.reserve(item.size());
          for (const auto &child : item)
            array->elements.push_back(toRuntime(child, heap));
          return protectedArray;
        } else if constexpr (std::is_same_v<T, FormatValue::Object>) {
          auto *object = heap.allocate();
          Value protectedObject(object);
          auto roots = heap.rootScope();
          roots.protect(protectedObject);
          for (const auto &[name, child] : item)
            object->fields.emplace(name, toRuntime(child, heap));
          return protectedObject;
        } else {
          return Value(item);
        }
      },
      value.data);
}

std::optional<FormatValue> fromRuntime(const Value &value, std::string &error,
                                       std::unordered_set<const void *> &active) {
  return std::visit(
      [&](const auto &item) -> std::optional<FormatValue> {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_same_v<T, bool> ||
                      std::is_same_v<T, std::int64_t> || std::is_same_v<T, double> ||
                      std::is_same_v<T, std::string>) {
          return FormatValue(item);
        } else if constexpr (std::is_same_v<T, ArrayPtr>) {
          if (!item)
            return FormatValue();
          if (!active.insert(item).second) {
            error = "document values cannot contain cyclic arrays";
            return std::nullopt;
          }
          FormatValue::Array result;
          result.reserve(item->elements.size());
          for (const auto &child : item->elements) {
            auto converted = fromRuntime(child, error, active);
            if (!converted) {
              active.erase(item);
              return std::nullopt;
            }
            result.push_back(std::move(*converted));
          }
          active.erase(item);
          return FormatValue(std::move(result));
        } else if constexpr (std::is_same_v<T, ObjectPtr>) {
          if (!item)
            return FormatValue();
          if (!active.insert(item).second) {
            error = "document values cannot contain cyclic objects";
            return std::nullopt;
          }
          FormatValue::Object result;
          for (const auto &[name, child] : item->fields) {
            auto converted = fromRuntime(child, error, active);
            if (!converted) {
              active.erase(item);
              return std::nullopt;
            }
            result.emplace(name, std::move(*converted));
          }
          active.erase(item);
          return FormatValue(std::move(result));
        } else {
          error = "document values support only null, booleans, numbers, strings, arrays, and objects; received " +
                  value.typeName();
          return std::nullopt;
        }
      },
      value.data);
}

} // namespace

Value formatValueToRuntime(const FormatValue &value, Heap &heap) { return toRuntime(value, heap); }

std::optional<FormatValue> runtimeValueToFormat(const Value &value, std::string &error) {
  std::unordered_set<const void *> active;
  return fromRuntime(value, error, active);
}

} // namespace kyna
