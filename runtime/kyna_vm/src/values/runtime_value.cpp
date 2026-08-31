#include "kyna/execution/runtime_object_model.hpp"
#include <sstream>

namespace kyna {
std::string Value::typeName() const {
  return std::visit(
      [](const auto &v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>)
          return "null";
        else if constexpr (std::is_same_v<T, bool>)
          return "bool";
        else if constexpr (std::is_same_v<T, int64_t>)
          return "int";
        else if constexpr (std::is_same_v<T, double>)
          return "float";
        else if constexpr (std::is_same_v<T, std::string>)
          return "str";
        else if constexpr (std::is_same_v<T, char>)
          return "char";
        else if constexpr (std::is_same_v<T, ObjectPtr>)
          return v && v->klass ? v->klass->declaration.name
                               : v && !v->vmClassName.empty() ? v->vmClassName : "object";
        else if constexpr (std::is_same_v<T, ArrayPtr>)
          return "array";
        else if constexpr (std::is_same_v<T, FunctionPtr>)
          return "func";
        else if constexpr (std::is_same_v<T, ModulePtr>)
          return "module";
        else if constexpr (std::is_same_v<T, VmFunctionReference>)
          return "func";
        else if constexpr (std::is_same_v<T, VmClosure *> ||
                           std::is_same_v<T, VmBoundMethod *>)
          return "func";
        else if constexpr (std::is_same_v<T, ErrorPtr>)
          return "Error";
        else
          return "class";
      },
      data);
}
std::string Value::display() const {
  return std::visit(
      [](const auto &v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>)
          return "null";
        else if constexpr (std::is_same_v<T, bool>)
          return v ? "true" : "false";
        else if constexpr (std::is_same_v<T, int64_t>)
          return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) {
          std::ostringstream o;
          o << v;
          return o.str();
        } else if constexpr (std::is_same_v<T, std::string>)
          return v;
        else if constexpr (std::is_same_v<T, char>)
          return std::string(1, v);
        else if constexpr (std::is_same_v<T, ObjectPtr>)
          return "<" + (v && v->klass ? v->klass->declaration.name
                                       : v && !v->vmClassName.empty() ? v->vmClassName
                                                                      : "object") +
                 ">";
        else if constexpr (std::is_same_v<T, ArrayPtr>) {
          std::string s = "[";
          for (size_t i = 0; i < v->elements.size(); ++i) {
            if (i)
              s += ", ";
            s += v->elements[i].display();
          }
          return s + "]";
        } else if constexpr (std::is_same_v<T, FunctionPtr>)
          return "<func>";
        else if constexpr (std::is_same_v<T, ModulePtr>)
          return "<module " + (v ? v->displayName : std::string("unknown")) + ">";
        else if constexpr (std::is_same_v<T, VmFunctionReference>)
          return "<func @" + std::to_string(v.function) + ">";
        else if constexpr (std::is_same_v<T, VmClosure *>)
          return v ? "<closure @" + std::to_string(v->function) + ">" : "<closure>";
        else if constexpr (std::is_same_v<T, VmBoundMethod *>)
          return v ? "<bound method @" + std::to_string(v->function) + ">"
                   : "<bound method>";
        else if constexpr (std::is_same_v<T, ErrorPtr>)
          return v ? v->message : "<error>";
        else
          return "<class " + v->declaration.name + ">";
      },
      data);
}
bool Value::isTruthy() const {
  if (std::holds_alternative<std::nullptr_t>(data))
    return false;
  if (auto p = std::get_if<bool>(&data))
    return *p;
  if (auto p = std::get_if<int64_t>(&data))
    return *p != 0;
  if (auto p = std::get_if<double>(&data))
    return *p != 0.0;
  return true;
}
bool Value::equals(const Value &other) const {
  if ((std::holds_alternative<int64_t>(data) || std::holds_alternative<double>(data)) &&
      (std::holds_alternative<int64_t>(other.data) || std::holds_alternative<double>(other.data))) {
    double left = std::holds_alternative<int64_t>(data)
                      ? static_cast<double>(std::get<int64_t>(data))
                      : std::get<double>(data);
    double right = std::holds_alternative<int64_t>(other.data)
                       ? static_cast<double>(std::get<int64_t>(other.data))
                       : std::get<double>(other.data);
    return left == right;
  }
  if (data.index() != other.data.index())
    return false;
  return std::visit(
      [](const auto &a, const auto &b) -> bool {
        using A = std::decay_t<decltype(a)>;
        using B = std::decay_t<decltype(b)>;
        if constexpr (std::is_same_v<A, B>) {
          if constexpr (std::is_same_v<A, std::nullptr_t>)
            return true;
          else
            return a == b;
        }
        return false;
      },
      data, other.data);
}
} // namespace kyna
