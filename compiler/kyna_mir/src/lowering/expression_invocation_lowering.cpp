#include "hir_lowering_private.hpp"
#include <type_traits>

namespace kyna::mir_lowering_detail {

MirTemporary HirLowerer::lowerInvocation(const HirExpression &expression) {
  return std::visit(
      [&](const auto &node) -> MirTemporary {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, HirAssignIndexExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> operands{lowerExpression(node.object),
                                             lowerExpression(node.index),
                                             lowerExpression(node.value)};
          current().instructions.push_back({MirInstructionKind::StoreIndex, target, {}, {},
                                             nullptr, expression.span, 0, std::move(operands)});
          return target;
        } else if constexpr (std::is_same_v<T, HirAssignMemberExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> operands{lowerExpression(node.object),
                                             lowerExpression(node.value)};
          current().instructions.push_back({MirInstructionKind::StoreMember, target, {}, {},
                                             node.member, expression.span, 0, std::move(operands)});
          return target;
        } else if constexpr (std::is_same_v<T, HirMemberExpression>) {
          const auto target = temporary();
          const auto object = lowerExpression(node.object);
          current().instructions.push_back({MirInstructionKind::LoadMember, target, object, {},
                                             node.member, expression.span, 0, {}});
          return target;
        } else if constexpr (std::is_same_v<T, HirBoundMethodExpression>) {
          const auto target = temporary();
          const auto receiver = lowerExpression(node.receiver);
          current().instructions.push_back({MirInstructionKind::BindMethod, target, receiver, {},
                                             nullptr, expression.span, node.function.value + 1, {}});
          return target;
        } else if constexpr (std::is_same_v<T, HirNativeCallExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> arguments;
          arguments.reserve(node.arguments.size());
          for (const auto argument : node.arguments)
            arguments.push_back(lowerExpression(argument));
          current().instructions.push_back({MirInstructionKind::CallNative, target, {}, {},
                                             node.name, expression.span, 0, std::move(arguments)});
          return target;
        } else if constexpr (std::is_same_v<T, HirIndexExpression>) {
          const auto target = temporary();
          const auto object = lowerExpression(node.object);
          const auto index = lowerExpression(node.index);
          current().instructions.push_back({MirInstructionKind::LoadIndex, target, object, index,
                                             nullptr, expression.span, 0, {}});
          return target;
        } else if constexpr (std::is_same_v<T, HirArrayExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> elements;
          elements.reserve(node.elements.size());
          for (const auto element : node.elements)
            elements.push_back(lowerExpression(element));
          current().instructions.push_back({MirInstructionKind::MakeArray, target, {}, {}, nullptr,
                                             expression.span, 0, std::move(elements)});
          return target;
        } else if constexpr (std::is_same_v<T, HirObjectExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> values;
          std::vector<std::string> names;
          values.reserve(node.fields.size());
          names.reserve(node.fields.size());
          for (const auto &field : node.fields) {
            names.push_back(field.name);
            values.push_back(lowerExpression(field.value));
          }
          current().instructions.push_back({MirInstructionKind::MakeObject, target, {}, {}, nullptr,
                                             expression.span, 0, std::move(values), 0, {},
                                             std::move(names)});
          return target;
        } else if constexpr (std::is_same_v<T, HirNewExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> arguments;
          arguments.reserve(node.arguments.size());
          for (const auto argument : node.arguments)
            arguments.push_back(lowerExpression(argument));
          current().instructions.push_back({MirInstructionKind::MakeInstance, target, {}, {},
                                             nullptr, expression.span, node.klass.value,
                                             std::move(arguments)});
          return target;
        } else if constexpr (std::is_same_v<T, HirClosureExpression>) {
          const auto target = temporary();
          MirInstruction instruction;
          instruction.kind = MirInstructionKind::Closure;
          instruction.destination = target;
          instruction.function = node.function.value + 1;
          instruction.span = expression.span;
          for (const auto capture : hir.functions.at(node.function.value).captures) {
            if (const auto local = locals.find(capture.value); local != locals.end())
              instruction.captureSources.push_back(
                  {MirCaptureSource::Kind::Local, local->second.value});
            else
              instruction.captureSources.push_back(
                  {MirCaptureSource::Kind::Capture, captureIndexes.at(capture.value)});
          }
          current().instructions.push_back(std::move(instruction));
          return target;
        } else if constexpr (std::is_same_v<T, HirAssignLocalExpression>) {
          const auto source = lowerExpression(node.value);
          if (const auto local = locals.find(node.local.value); local != locals.end()) {
            current().instructions.push_back(
                {MirInstructionKind::Move, local->second, source, {}, nullptr, expression.span, 0,
                 {}});
            return local->second;
          }
          MirInstruction instruction;
          instruction.kind = MirInstructionKind::StoreCapture;
          instruction.destination = source;
          instruction.first = source;
          instruction.capture = captureIndexes.at(node.local.value);
          instruction.span = expression.span;
          current().instructions.push_back(std::move(instruction));
          return source;
        } else if constexpr (std::is_same_v<T, HirCallExpression>) {
          std::vector<MirTemporary> arguments;
          arguments.reserve(node.arguments.size());
          for (const auto argument : node.arguments)
            arguments.push_back(lowerExpression(argument));
          const auto target = temporary();
          MirInstruction instruction;
          instruction.kind = MirInstructionKind::Call;
          instruction.destination = target;
          instruction.span = expression.span;
          instruction.function = node.function.value + 1;
          instruction.arguments = std::move(arguments);
          current().instructions.push_back(std::move(instruction));
          return target;
        } else if constexpr (std::is_same_v<T, HirIndirectCallExpression>) {
          const auto callee = lowerExpression(node.callee);
          std::vector<MirTemporary> arguments;
          arguments.reserve(node.arguments.size());
          for (const auto argument : node.arguments)
            arguments.push_back(lowerExpression(argument));
          const auto target = temporary();
          MirInstruction instruction;
          instruction.kind = MirInstructionKind::CallIndirect;
          instruction.destination = target;
          instruction.first = callee;
          instruction.span = expression.span;
          instruction.arguments = std::move(arguments);
          current().instructions.push_back(std::move(instruction));
          return target;
        } else
          return temporary();
      },
      expression.node);
}

} // namespace kyna::mir_lowering_detail
