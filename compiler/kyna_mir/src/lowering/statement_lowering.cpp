#include "hir_lowering_private.hpp"
#include <type_traits>

namespace kyna::mir_lowering_detail {

bool HirLowerer::lowerActiveCleanups() {
  const auto saved = cleanups;
  for (std::size_t count = saved.size(); count > 0; --count) {
    cleanups.assign(saved.begin(), saved.begin() + static_cast<std::ptrdiff_t>(count - 1));
    lowerStatement(saved[count - 1]);
    if (current().terminator) {
      cleanups = saved;
      return false;
    }
  }
  cleanups = saved;
  return true;
}

std::vector<MirBlockId> HirLowerer::blocksFrom(std::size_t first) const {
  std::vector<MirBlockId> result;
  result.reserve(activeBlocks->size() - first);
  for (std::size_t index = first; index < activeBlocks->size(); ++index)
    result.push_back(MirBlockId{static_cast<std::uint32_t>(index)});
  return result;
}

void HirLowerer::lowerStatement(HirStatementId id) {
  if (current().terminator)
    return;
  const auto &statement = hir.statements.at(id.value);
  std::visit(
      [&](const auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, HirBindLocalStatement>) {
          const auto destination = temporary();
          locals.insert_or_assign(node.local.value, destination);
          const auto source = lowerExpression(node.initializer);
          current().instructions.push_back(
              {MirInstructionKind::Move, destination, source, {}, nullptr, statement.span, 0,
               {}});
        } else if constexpr (std::is_same_v<T, HirEvaluateStatement>) {
          lowerExpression(node.expression);
        } else if constexpr (std::is_same_v<T, HirReturnStatement>) {
          const auto value = lowerExpression(node.expression);
          if (lowerActiveCleanups())
            terminate(MirReturnTerminator{value}, statement.span);
        } else if constexpr (std::is_same_v<T, HirBlockStatement>) {
          for (const auto child : node.statements)
            lowerStatement(child);
        } else if constexpr (std::is_same_v<T, HirIfStatement>) {
          const auto condition = lowerExpression(node.condition);
          const auto thenBlock = addBlock();
          const auto elseBlock = addBlock();
          const auto continuation = addBlock();
          terminate(MirBranchTerminator{condition, thenBlock, elseBlock}, statement.span);

          currentBlock = thenBlock;
          lowerStatement(node.thenBranch);
          if (!current().terminator)
            terminate(MirGotoTerminator{continuation}, statement.span);

          currentBlock = elseBlock;
          if (node.elseBranch)
            lowerStatement(*node.elseBranch);
          if (!current().terminator)
            terminate(MirGotoTerminator{continuation}, statement.span);
          currentBlock = continuation;
        } else if constexpr (std::is_same_v<T, HirWhileStatement>) {
          const auto conditionBlock = addBlock();
          const auto bodyBlock = addBlock();
          const auto exitBlock = addBlock();
          terminate(MirGotoTerminator{conditionBlock}, statement.span);

          currentBlock = conditionBlock;
          const auto condition = lowerExpression(node.condition);
          terminate(MirBranchTerminator{condition, bodyBlock, exitBlock}, statement.span);

          currentBlock = bodyBlock;
          loops.push_back({node.label, exitBlock, conditionBlock});
          breaks.push_back(exitBlock);
          lowerStatement(node.body);
          breaks.pop_back();
          loops.pop_back();
          if (!current().terminator)
            terminate(MirGotoTerminator{conditionBlock}, statement.span);
          currentBlock = exitBlock;
        } else if constexpr (std::is_same_v<T, HirLoopStatement>) {
          if (node.initializer)
            lowerStatement(*node.initializer);
          if (current().terminator)
            return;
          const auto conditionBlock = addBlock();
          const auto bodyBlock = addBlock();
          const auto incrementBlock = addBlock();
          const auto exitBlock = addBlock();
          terminate(MirGotoTerminator{conditionBlock}, statement.span);

          currentBlock = conditionBlock;
          const auto condition = lowerExpression(node.condition);
          terminate(MirBranchTerminator{condition, bodyBlock, exitBlock}, statement.span);

          currentBlock = bodyBlock;
          loops.push_back({node.label, exitBlock, incrementBlock});
          breaks.push_back(exitBlock);
          lowerStatement(node.body);
          breaks.pop_back();
          loops.pop_back();
          if (!current().terminator)
            terminate(MirGotoTerminator{incrementBlock}, statement.span);

          currentBlock = incrementBlock;
          if (node.increment)
            lowerExpression(*node.increment);
          terminate(MirGotoTerminator{conditionBlock}, statement.span);
          currentBlock = exitBlock;
        } else if constexpr (std::is_same_v<T, HirBreakStatement>) {
          const auto target = breakTarget(node.label);
          if (lowerActiveCleanups())
            terminate(MirGotoTerminator{target}, statement.span);
        } else if constexpr (std::is_same_v<T, HirContinueStatement>) {
          const auto target = loopTarget(node.label).continueTarget;
          if (lowerActiveCleanups())
            terminate(MirGotoTerminator{target}, statement.span);
        } else if constexpr (std::is_same_v<T, HirThrowStatement>) {
          terminate(MirThrowTerminator{lowerExpression(node.value)}, statement.span);
        } else if constexpr (std::is_same_v<T, HirSwitchStatement>) {
          const auto subject = temporary();
          const auto subjectValue = lowerExpression(node.subject);
          current().instructions.push_back(
              {MirInstructionKind::Move, subject, subjectValue, {}, nullptr, statement.span, 0,
               {}});

          std::vector<std::size_t> valueArms;
          std::optional<std::size_t> defaultArm;
          for (std::size_t arm = 0; arm < node.cases.size(); ++arm) {
            if (node.cases[arm].value)
              valueArms.push_back(arm);
            else
              defaultArm = arm;
          }

          const auto continuation = addBlock();
          std::vector<MirBlockId> armBlocks(node.cases.size());
          for (auto &arm : armBlocks)
            arm = addBlock();
          std::vector<MirBlockId> testBlocks(valueArms.size());
          for (auto &test : testBlocks)
            test = addBlock();

          if (valueArms.empty()) {
            terminate(MirGotoTerminator{armBlocks[*defaultArm]}, statement.span);
          } else {
            terminate(MirGotoTerminator{testBlocks.front()}, statement.span);
            for (std::size_t arm = 0; arm < valueArms.size(); ++arm) {
              currentBlock = testBlocks[arm];
              const auto caseValue = lowerExpression(*node.cases[valueArms[arm]].value);
              const auto equal = temporary();
              current().instructions.push_back(
                  {MirInstructionKind::Equal, equal, subject, caseValue, {}, statement.span, 0,
                   {}});
              const auto whenFalse = arm + 1 < valueArms.size()
                                         ? testBlocks[arm + 1]
                                         : defaultArm ? armBlocks[*defaultArm] : continuation;
              terminate(MirBranchTerminator{equal, armBlocks[valueArms[arm]], whenFalse},
                        statement.span);
            }
          }

          breaks.push_back(continuation);
          for (std::size_t arm = 0; arm < node.cases.size(); ++arm) {
            currentBlock = armBlocks[arm];
            lowerStatement(node.cases[arm].body);
            if (!current().terminator)
              terminate(MirGotoTerminator{continuation}, statement.span);
          }
          breaks.pop_back();
          currentBlock = continuation;
        } else {
          const auto continuation = addBlock();
          const auto tryEntry = addBlock();
          terminate(MirGotoTerminator{tryEntry}, statement.span);
          currentBlock = tryEntry;
          const auto tryFirstBlock = static_cast<std::size_t>(tryEntry.value);

          if (node.finallyBranch)
            cleanups.push_back(*node.finallyBranch);
          lowerStatement(node.tryBranch);
          if (node.finallyBranch)
            cleanups.pop_back();
          const auto protectedTryBlocks = blocksFrom(tryFirstBlock);
          if (!current().terminator) {
            if (node.finallyBranch)
              lowerStatement(*node.finallyBranch);
            if (!current().terminator)
              terminate(MirGotoTerminator{continuation}, statement.span);
          }

          const auto error = temporary();
          const auto handler = addBlock();
          activeExceptionRegions->push_back({protectedTryBlocks, handler, error});
          currentBlock = handler;

          if (node.catchBranch) {
            const auto catchFirstBlock = static_cast<std::size_t>(handler.value);
            locals.insert_or_assign(node.catchLocal->value, error);
            if (node.finallyBranch)
              cleanups.push_back(*node.finallyBranch);
            lowerStatement(*node.catchBranch);
            if (node.finallyBranch)
              cleanups.pop_back();
            const auto protectedCatchBlocks = blocksFrom(catchFirstBlock);
            if (!current().terminator) {
              if (node.finallyBranch)
                lowerStatement(*node.finallyBranch);
              if (!current().terminator)
                terminate(MirGotoTerminator{continuation}, statement.span);
            }
            if (node.finallyBranch) {
              const auto rethrown = temporary();
              const auto rethrowHandler = addBlock();
              activeExceptionRegions->push_back(
                  {protectedCatchBlocks, rethrowHandler, rethrown});
              currentBlock = rethrowHandler;
              lowerStatement(*node.finallyBranch);
              if (!current().terminator)
                terminate(MirThrowTerminator{rethrown}, statement.span);
            }
          } else {
            if (node.finallyBranch)
              lowerStatement(*node.finallyBranch);
            if (!current().terminator)
              terminate(MirThrowTerminator{error}, statement.span);
          }
          currentBlock = continuation;
        }
      },
      statement.node);
}

} // namespace kyna::mir_lowering_detail
