#pragma once
#include "kyna/syntax/legacy_syntax_handles.hpp"
#include "kyna/diagnostics.hpp"
#include "kyna/memory/tracing_heap.hpp"
#include <functional>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <variant>
#include <vector>

namespace kyna {
struct Object;
struct Array;
struct Function;
struct Class;
struct ModuleNamespace;
struct VmClosure;
struct VmBoundMethod;
struct ErrorObject;
struct VmFunctionReference {
  std::uint32_t function{0};
  auto operator<=>(const VmFunctionReference &) const = default;
};
using ObjectPtr = Object *;
using ArrayPtr = Array *;
using FunctionPtr = std::shared_ptr<Function>;
using ClassPtr = std::shared_ptr<Class>;
using ModulePtr = std::shared_ptr<ModuleNamespace>;
using ErrorPtr = ErrorObject *;
struct Value {
  using Data = std::variant<std::nullptr_t, bool, int64_t, double, std::string, char, ObjectPtr,
                            ArrayPtr, FunctionPtr, ClassPtr, ModulePtr, VmFunctionReference,
                            VmClosure *, VmBoundMethod *, ErrorPtr>;
  Data data{nullptr};
  Value() = default;
  template <class T> Value(T v) : data(std::move(v)) {}
  std::string typeName() const;
  std::string display() const;
  bool isTruthy() const;
  bool equals(const Value &) const;
};
struct VmCaptureCell {
  Value value;
};
struct VmClosure {
  std::uint32_t function{0};
  std::vector<VmCaptureCell *> captures;
};
struct VmBoundMethod {
  ObjectPtr receiver{nullptr};
  std::uint32_t function{0};
};
struct ErrorObject {
  std::string message;
  std::string code;
  Value cause;
};
class RuntimeThrownError : public std::exception {
public:
  explicit RuntimeThrownError(ErrorPtr thrown) : value(thrown) {}
  [[nodiscard]] const char *what() const noexcept override {
    return value ? value->message.c_str() : "Kyna error";
  }
  ErrorPtr value{nullptr};
  std::vector<RuntimeCallFrame> frames;
};
struct Cell {
  Value value;
  bool mutableBinding{false};
};
class Environment : public std::enable_shared_from_this<Environment> {
public:
  explicit Environment(std::shared_ptr<Environment> parent = nullptr);
  void define(const std::string &, Value, bool);
  Cell &get(const std::string &);
  void assign(const std::string &, Value);
  std::shared_ptr<Environment> parent() const;

private:
  std::map<std::string, Cell> values;
  std::shared_ptr<Environment> enclosing;
  friend class Heap;
};
struct Object {
  std::map<std::string, Value> fields;
  ClassPtr klass;
  std::optional<std::uint32_t> vmClass;
  std::string vmClassName;
};
struct Array {
  std::vector<Value> elements;
};
struct Function {
  FunctionDecl declaration;
  std::shared_ptr<Environment> closure;
  ObjectPtr boundThis;
  bool native{false};
  std::function<Value(const std::vector<Value> &)> nativeCall;
  Value call(const std::vector<Value> &, class Interpreter &);
};
struct Class {
  ClassDecl declaration;
  ClassPtr parent;
  std::map<std::string, FunctionPtr> methods;
  std::map<std::string, Value> staticFields;
  FunctionPtr findMethod(const std::string &) const;
};
struct ModuleNamespace {
  std::shared_ptr<Environment> environment;
  std::set<std::string> exports;
  std::string displayName;
};
} // namespace kyna
