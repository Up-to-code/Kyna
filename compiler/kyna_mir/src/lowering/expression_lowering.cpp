#include "hir_lowering_private.hpp"
#include <type_traits>

namespace kyna::mir_lowering_detail {

MirTemporary HirLowerer::lowerExpression(HirExpressionId id) {
  const auto &expression = hir.expressions.at(id.value);
  const auto result = std::visit(
      [&](const auto &node) -> MirTemporary {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, HirConstantExpression>) {
          const auto target = temporary();
          current().instructions.push_back(
              {MirInstructionKind::Constant, target, {}, {}, node.value, expression.span, 0, {}});
          return target;
        } else if constexpr (std::is_same_v<T, HirLocalExpression>) {
          if (const auto local = locals.find(node.local.value); local != locals.end())
            return local->second;
          const auto target = temporary();
          MirInstruction instruction;
          instruction.kind = MirInstructionKind::LoadCapture;
          instruction.destination = target;
          instruction.capture = captureIndexes.at(node.local.value);
          instruction.span = expression.span;
          current().instructions.push_back(std::move(instruction));
          return target;
        } else if constexpr (std::is_same_v<T, HirFunctionReferenceExpression>) {
          const auto target = temporary();
          MirInstruction instruction;
          instruction.kind = MirInstructionKind::FunctionReference;
          instruction.destination = target;
          instruction.span = expression.span;
          instruction.function = node.function.value + 1;
          current().instructions.push_back(std::move(instruction));
          return target;
        } else if constexpr (std::is_same_v<T, HirAssignIndexExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> operands{lowerExpression(node.object),
                                             lowerExpression(node.index),
                                             lowerExpression(node.value)};
          current().instructions.push_back({MirInstructionKind::StoreIndex, target, {}, {},
                                             nullptr, expression.span, 0,
                                             std::move(operands)});
          return target;
        } else if constexpr (std::is_same_v<T, HirAssignMemberExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> operands{lowerExpression(node.object),
                                             lowerExpression(node.value)};
          current().instructions.push_back({MirInstructionKind::StoreMember, target, {}, {},
                                             node.member, expression.span, 0,
                                             std::move(operands)});
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
          current().instructions.push_back({MirInstructionKind::BindMethod, target, receiver,
                                             {}, nullptr, expression.span,
                                             node.function.value + 1, {}});
          return target;
        } else if constexpr (std::is_same_v<T, HirNativeCallExpression>) {
          const auto target = temporary();
          std::vector<MirTemporary> arguments;
          arguments.reserve(node.arguments.size());
          for (const auto argument : node.arguments)
            arguments.push_back(lowerExpression(argument));
          current().instructions.push_back({MirInstructionKind::CallNative, target, {}, {},
                                             node.name, expression.span, 0,
                                             std::move(arguments)});
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
          current().instructions.push_back({MirInstructionKind::MakeArray, target, {}, {},
                                             nullptr, expression.span, 0,
                                             std::move(elements)});
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
          current().instructions.push_back(
              {MirInstructionKind::MakeObject, target, {}, {}, nullptr, expression.span, 0,
               std::move(values), 0, {}, std::move(names)});
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
        } else if constexpr (std::is_same_v<T, HirUnaryExpression>) {
          const auto operand = lowerExpression(node.operand);
          const auto target = temporary();
          current().instructions.push_back(
              {node.operation == HirUnaryOperator::Negate ? MirInstructionKind::Negate
                                                           : MirInstructionKind::Not,
               target, operand, {}, nullptr, expression.span, 0, {}});
          return target;
        } else if constexpr (std::is_same_v<T, HirAwaitExpression>) {
          const auto operand = lowerExpression(node.operand);
          const auto target = temporary();
          current().instructions.push_back(
              {MirInstructionKind::Move, target, operand, {}, nullptr, expression.span, 0, {}});
          return target;
        } else if constexpr (std::is_same_v<T, HirBinaryExpression>) {
          const auto left = lowerExpression(node.left);
          if (node.operation == HirBinaryOperator::And ||
              node.operation == HirBinaryOperator::Or) {
            const auto target = temporary();
            const auto rightBlock = addBlock();
            const auto shortCircuitBlock = addBlock();
            const auto continuationBlock = addBlock();
            const bool isAnd = node.operation == HirBinaryOperator::And;
            terminate(MirBranchTerminator{left,
                                          isAnd ? rightBlock : shortCircuitBlock,
                                          isAnd ? shortCircuitBlock : rightBlock},
                      expression.span);

            currentBlock = shortCircuitBlock;
            const auto shortCircuitValue = temporary();
            current().instructions.push_back(
                {MirInstructionKind::Constant, shortCircuitValue, {}, {}, !isAnd,
                 expression.span, 0, {}});
            current().instructions.push_back(
                {MirInstructionKind::Move, target, shortCircuitValue, {}, nullptr,
                 expression.span, 0, {}});
            terminate(MirGotoTerminator{continuationBlock}, expression.span);

            currentBlock = rightBlock;
            const auto right = lowerExpression(node.right);
            const auto inverted = temporary();
            const auto booleanRight = temporary();
            current().instructions.push_back(
                {MirInstructionKind::Not, inverted, right, {}, nullptr, expression.span, 0, {}});
            current().instructions.push_back(
                {MirInstructionKind::Not, booleanRight, inverted, {}, nullptr, expression.span, 0,
                 {}});
            current().instructions.push_back(
                {MirInstructionKind::Move, target, booleanRight, {}, nullptr, expression.span, 0,
                 {}});
            terminate(MirGotoTerminator{continuationBlock}, expression.span);

            currentBlock = continuationBlock;
            return target;
          }
          const auto right = lowerExpression(node.right);
          const auto target = temporary();
          current().instructions.push_back(
              {instructionFor(node.operation), target, left, right, nullptr, expression.span, 0,
               {}});
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
        } else if constexpr (std::is_same_v<T, HirIfExpression>) {
          const auto condition = lowerExpression(node.condition);
          const auto target = temporary();
          const auto thenBlock = addBlock();
          const auto elseBlock = addBlock();
          const auto continuationBlock = addBlock();
          terminate(MirBranchTerminator{condition, thenBlock, elseBlock}, expression.span);

          currentBlock = thenBlock;
          lowerStatement(node.thenPrelude);
          if (!current().terminator) {
            const auto value = lowerExpression(node.thenValue);
            current().instructions.push_back(
                {MirInstructionKind::Move, target, value, {}, nullptr, expression.span, 0, {}});
            terminate(MirGotoTerminator{continuationBlock}, expression.span);
          }

          currentBlock = elseBlock;
          lowerStatement(node.elsePrelude);
          if (!current().terminator) {
            const auto value = lowerExpression(node.elseValue);
            current().instructions.push_back(
                {MirInstructionKind::Move, target, value, {}, nullptr, expression.span, 0, {}});
            terminate(MirGotoTerminator{continuationBlock}, expression.span);
          }
          currentBlock = continuationBlock;
          return target;
        } else {
          const auto subject = lowerExpression(node.subject);
          const auto target = temporary();
          const auto continuationBlock = addBlock();
          bool endedWithWildcard = false;
          for (const auto &arm : node.arms) {
            if (!arm.pattern) {
              const auto value = lowerExpression(arm.value);
              current().instructions.push_back(
                  {MirInstructionKind::Move, target, value, {}, nullptr, expression.span, 0,
                   {}});
              terminate(MirGotoTerminator{continuationBlock}, expression.span);
              endedWithWildcard = true;
              break;
            }
            const auto pattern = lowerExpression(*arm.pattern);
            const auto matches = temporary();
            current().instructions.push_back(
                {MirInstructionKind::Equal, matches, subject, pattern, nullptr, expression.span,
                 0, {}});
            const auto armBlock = addBlock();
            const auto nextArmBlock = addBlock();
            terminate(MirBranchTerminator{matches, armBlock, nextArmBlock}, expression.span);

            currentBlock = armBlock;
            const auto value = lowerExpression(arm.value);
            current().instructions.push_back(
                {MirInstructionKind::Move, target, value, {}, nullptr, expression.span, 0, {}});
            terminate(MirGotoTerminator{continuationBlock}, expression.span);
            currentBlock = nextArmBlock;
          }
          if (!endedWithWildcard && !current().terminator) {
            const auto fallback = temporary();
            current().instructions.push_back(
                {MirInstructionKind::Constant, fallback, {}, {}, nullptr, expression.span, 0,
                 {}});
            current().instructions.push_back(
                {MirInstructionKind::Move, target, fallback, {}, nullptr, expression.span, 0,
                 {}});
            terminate(MirGotoTerminator{continuationBlock}, expression.span);
          }
          currentBlock = continuationBlock;
          return target;
        }
      },
      expression.node);
  return result;
}

} // namespace kyna::mir_lowering_detail
