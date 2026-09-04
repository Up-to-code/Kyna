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
            terminate(MirBranchTerminator{left, isAnd ? rightBlock : shortCircuitBlock,
                                          isAnd ? shortCircuitBlock : rightBlock},
                      expression.span);

            currentBlock = shortCircuitBlock;
            const auto shortCircuitValue = temporary();
            current().instructions.push_back({MirInstructionKind::Constant, shortCircuitValue, {},
                                               {}, !isAnd, expression.span, 0, {}});
            current().instructions.push_back({MirInstructionKind::Move, target, shortCircuitValue,
                                               {}, nullptr, expression.span, 0, {}});
            terminate(MirGotoTerminator{continuationBlock}, expression.span);

            currentBlock = rightBlock;
            const auto right = lowerExpression(node.right);
            const auto inverted = temporary();
            const auto booleanRight = temporary();
            current().instructions.push_back(
                {MirInstructionKind::Not, inverted, right, {}, nullptr, expression.span, 0, {}});
            current().instructions.push_back({MirInstructionKind::Not, booleanRight, inverted, {},
                                               nullptr, expression.span, 0, {}});
            current().instructions.push_back({MirInstructionKind::Move, target, booleanRight, {},
                                               nullptr, expression.span, 0, {}});
            terminate(MirGotoTerminator{continuationBlock}, expression.span);

            currentBlock = continuationBlock;
            return target;
          }
          const auto right = lowerExpression(node.right);
          const auto target = temporary();
          current().instructions.push_back({instructionFor(node.operation), target, left, right,
                                             nullptr, expression.span, 0, {}});
          return target;
        } else if constexpr (std::is_same_v<T, HirIfExpression>)
          return lowerIfExpression(node, expression.span);
        else if constexpr (std::is_same_v<T, HirMatchExpression>)
          return lowerMatchExpression(node, expression.span);
        else
          return lowerInvocation(expression);
      },
      expression.node);
  return result;
}

} // namespace kyna::mir_lowering_detail
