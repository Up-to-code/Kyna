#include "kyna/hir/hir_renderer.hpp"
#include <sstream>
#include <type_traits>

namespace kyna {
namespace {
std::string constantText(const HirConstant &constant) {
  return std::visit(
      [](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>)
          return "null";
        else if constexpr (std::is_same_v<T, bool>)
          return value ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::string>)
          return '"' + value + '"';
        else if constexpr (std::is_same_v<T, char>)
          return std::string("'") + value + "'";
        else
          return std::to_string(value);
      },
      constant);
}
} // namespace

std::string renderHir(const HirProgram &program) {
  std::ostringstream output;
  output << "hir.module " << program.name << '\n';
  for (std::size_t index = 0; index < program.locals.size(); ++index)
    output << "  local %l" << index << ' ' << program.locals[index].name << ' '
           << (program.locals[index].mutableBinding ? "mutable" : "immutable") << '\n';
  for (std::size_t index = 0; index < program.expressions.size(); ++index) {
    output << "  expr %e" << index << " = ";
    std::visit(
        [&](const auto &node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, HirConstantExpression>)
            output << "constant " << constantText(node.value);
          else if constexpr (std::is_same_v<T, HirLocalExpression>)
            output << "local %l" << node.local.value;
          else if constexpr (std::is_same_v<T, HirFunctionReferenceExpression>)
            output << "function @f" << node.function.value;
          else if constexpr (std::is_same_v<T, HirClosureExpression>)
            output << "closure @f" << node.function.value;
          else if constexpr (std::is_same_v<T, HirUnaryExpression>)
            output << hirUnaryOperatorName(node.operation) << " %e" << node.operand.value;
          else if constexpr (std::is_same_v<T, HirBinaryExpression>)
            output << hirBinaryOperatorName(node.operation) << " %e" << node.left.value << ", %e"
                   << node.right.value;
          else if constexpr (std::is_same_v<T, HirAssignLocalExpression>)
            output << "assign %l" << node.local.value << ", %e" << node.value.value;
          else if constexpr (std::is_same_v<T, HirAssignIndexExpression>)
            output << "assign.index %e" << node.object.value << ", %e" << node.index.value
                   << ", %e" << node.value.value;
          else if constexpr (std::is_same_v<T, HirAssignMemberExpression>)
            output << "assign.member %e" << node.object.value << ", " << node.member
                   << ", %e" << node.value.value;
          else if constexpr (std::is_same_v<T, HirCallExpression>) {
            output << "call @f" << node.function.value << '(';
            for (std::size_t argument = 0; argument < node.arguments.size(); ++argument) {
              if (argument) output << ", ";
              output << "%e" << node.arguments[argument].value;
            }
            output << ')';
          } else if constexpr (std::is_same_v<T, HirIndirectCallExpression>) {
            output << "call.indirect %e" << node.callee.value << '(';
            for (std::size_t argument = 0; argument < node.arguments.size(); ++argument) {
              if (argument) output << ", ";
              output << "%e" << node.arguments[argument].value;
            }
            output << ')';
          } else if constexpr (std::is_same_v<T, HirNativeCallExpression>) {
            output << "call.native " << node.name << '(';
            for (std::size_t argument = 0; argument < node.arguments.size(); ++argument) {
              if (argument) output << ", ";
              output << "%e" << node.arguments[argument].value;
            }
            output << ')';
          } else if constexpr (std::is_same_v<T, HirNewExpression>) {
            output << "new @c" << node.klass.value << '(';
            for (std::size_t argument = 0; argument < node.arguments.size(); ++argument) {
              if (argument) output << ", ";
              output << "%e" << node.arguments[argument].value;
            }
            output << ')';
          } else if constexpr (std::is_same_v<T, HirMemberExpression>) {
            output << "member %e" << node.object.value << ", " << node.member;
          } else if constexpr (std::is_same_v<T, HirBoundMethodExpression>) {
            output << "bind.method %e" << node.receiver.value << ", @f"
                   << node.function.value;
          } else if constexpr (std::is_same_v<T, HirIndexExpression>) {
            output << "index %e" << node.object.value << ", %e" << node.index.value;
          } else if constexpr (std::is_same_v<T, HirArrayExpression>) {
            output << "array [";
            for (std::size_t index = 0; index < node.elements.size(); ++index) {
              if (index) output << ", ";
              output << "%e" << node.elements[index].value;
            }
            output << ']';
          } else if constexpr (std::is_same_v<T, HirObjectExpression>) {
            output << "object {";
            for (std::size_t index = 0; index < node.fields.size(); ++index) {
              if (index) output << ", ";
              output << node.fields[index].name << ": %e" << node.fields[index].value.value;
            }
            output << '}';
          } else if constexpr (std::is_same_v<T, HirIfExpression>) {
            output << "if %e" << node.condition.value << " then %s"
                   << node.thenPrelude.value << " => %e" << node.thenValue.value << " else %s"
                   << node.elsePrelude.value << " => %e" << node.elseValue.value;
          } else {
            output << "match %e" << node.subject.value;
            for (const auto &arm : node.arms) {
              output << " [";
              if (arm.pattern)
                output << "%e" << arm.pattern->value;
              else
                output << '_';
              output << " => %e" << arm.value.value << ']';
            }
          }
        },
        program.expressions[index].node);
    output << '\n';
  }
  for (std::size_t index = 0; index < program.statements.size(); ++index) {
    output << "  stmt %s" << index << " = ";
    std::visit(
        [&](const auto &node) {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, HirBindLocalStatement>)
            output << "bind %l" << node.local.value << ", %e" << node.initializer.value;
          else if constexpr (std::is_same_v<T, HirEvaluateStatement>)
            output << "evaluate %e" << node.expression.value;
          else if constexpr (std::is_same_v<T, HirReturnStatement>)
            output << "return %e" << node.expression.value;
          else if constexpr (std::is_same_v<T, HirBlockStatement>) {
            output << "block";
            for (const auto statement : node.statements) output << " %s" << statement.value;
          } else if constexpr (std::is_same_v<T, HirIfStatement>) {
            output << "if %e" << node.condition.value << ", %s" << node.thenBranch.value;
            if (node.elseBranch) output << ", %s" << node.elseBranch->value;
          } else if constexpr (std::is_same_v<T, HirWhileStatement>) {
            output << "while %e" << node.condition.value << ", %s" << node.body.value;
            if (!node.label.empty()) output << " label=" << node.label;
          } else if constexpr (std::is_same_v<T, HirLoopStatement>) {
            output << "loop";
            if (node.initializer) output << " init=%s" << node.initializer->value;
            output << " condition=%e" << node.condition.value;
            if (node.increment) output << " increment=%e" << node.increment->value;
            output << " body=%s" << node.body.value;
            if (!node.label.empty()) output << " label=" << node.label;
          } else if constexpr (std::is_same_v<T, HirBreakStatement>) {
            output << "break";
            if (!node.label.empty()) output << ' ' << node.label;
          } else if constexpr (std::is_same_v<T, HirContinueStatement>) {
            output << "continue";
            if (!node.label.empty()) output << ' ' << node.label;
          } else if constexpr (std::is_same_v<T, HirThrowStatement>) {
            output << "throw %e" << node.value.value;
          } else {
            output << "try %s" << node.tryBranch.value;
            if (node.catchBranch)
              output << " catch %l" << node.catchLocal->value << " %s"
                     << node.catchBranch->value;
            if (node.finallyBranch)
              output << " finally %s" << node.finallyBranch->value;
          }
        },
        program.statements[index].node);
    output << '\n';
  }
  output << "  body";
  for (const auto statement : program.body) output << " %s" << statement.value;
  output << '\n';
  for (std::size_t index = 0; index < program.classes.size(); ++index) {
    const auto &klass = program.classes[index];
    output << "  class @c" << index << ' ' << klass.name;
    if (klass.parent) output << " extends @c" << klass.parent->value;
    output << '\n';
    for (const auto &field : klass.fields)
      output << "    field " << field.name << '\n';
    if (klass.constructor)
      output << "    constructor @f" << klass.constructor->value << '\n';
    for (const auto &method : klass.methods)
      output << "    method " << method.name << " @f" << method.function.value << '\n';
  }
  for (std::size_t index = 0; index < program.functions.size(); ++index) {
    const auto &function = program.functions[index];
    output << "  function @f" << index << ' ' << function.name << '(';
    for (std::size_t parameter = 0; parameter < function.parameters.size(); ++parameter) {
      if (parameter) output << ", ";
      output << "%l" << function.parameters[parameter].value;
    }
    output << ") %s" << function.body.value << '\n';
    if (!function.captures.empty()) {
      output << "    captures";
      for (const auto capture : function.captures) output << " %l" << capture.value;
      output << '\n';
    }
  }
  return output.str();
}

} // namespace kyna
