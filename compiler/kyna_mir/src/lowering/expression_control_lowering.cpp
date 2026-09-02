#include "hir_lowering_private.hpp"
#include <type_traits>

namespace kyna::mir_lowering_detail {

MirTemporary HirLowerer::lowerIfExpression(const HirIfExpression &node, SourceSpan span) {
  const auto condition = lowerExpression(node.condition);
  const auto target = temporary();
  const auto thenBlock = addBlock();
  const auto elseBlock = addBlock();
  const auto continuationBlock = addBlock();
  terminate(MirBranchTerminator{condition, thenBlock, elseBlock}, span);

  currentBlock = thenBlock;
  lowerStatement(node.thenPrelude);
  if (!current().terminator) {
    const auto value = lowerExpression(node.thenValue);
    current().instructions.push_back(
        {MirInstructionKind::Move, target, value, {}, nullptr, span, 0, {}});
    terminate(MirGotoTerminator{continuationBlock}, span);
  }

  currentBlock = elseBlock;
  lowerStatement(node.elsePrelude);
  if (!current().terminator) {
    const auto value = lowerExpression(node.elseValue);
    current().instructions.push_back(
        {MirInstructionKind::Move, target, value, {}, nullptr, span, 0, {}});
    terminate(MirGotoTerminator{continuationBlock}, span);
  }
  currentBlock = continuationBlock;
  return target;
}

MirTemporary HirLowerer::lowerMatchExpression(const HirMatchExpression &node, SourceSpan span) {
  const auto subject = lowerExpression(node.subject);
  const auto target = temporary();
  const auto continuationBlock = addBlock();
  bool endedWithWildcard = false;
  for (const auto &arm : node.arms) {
    if (!arm.pattern) {
      const auto value = lowerExpression(arm.value);
      current().instructions.push_back(
          {MirInstructionKind::Move, target, value, {}, nullptr, span, 0, {}});
      terminate(MirGotoTerminator{continuationBlock}, span);
      endedWithWildcard = true;
      break;
    }
    const auto pattern = lowerExpression(*arm.pattern);
    const auto matches = temporary();
    current().instructions.push_back(
        {MirInstructionKind::Equal, matches, subject, pattern, nullptr, span, 0, {}});
    const auto armBlock = addBlock();
    const auto nextArmBlock = addBlock();
    terminate(MirBranchTerminator{matches, armBlock, nextArmBlock}, span);

    currentBlock = armBlock;
    const auto value = lowerExpression(arm.value);
    current().instructions.push_back(
        {MirInstructionKind::Move, target, value, {}, nullptr, span, 0, {}});
    terminate(MirGotoTerminator{continuationBlock}, span);
    currentBlock = nextArmBlock;
  }
  if (!endedWithWildcard && !current().terminator) {
    const auto fallback = temporary();
    current().instructions.push_back(
        {MirInstructionKind::Constant, fallback, {}, {}, nullptr, span, 0, {}});
    current().instructions.push_back(
        {MirInstructionKind::Move, target, fallback, {}, nullptr, span, 0, {}});
    terminate(MirGotoTerminator{continuationBlock}, span);
  }
  currentBlock = continuationBlock;
  return target;
}

} // namespace kyna::mir_lowering_detail
