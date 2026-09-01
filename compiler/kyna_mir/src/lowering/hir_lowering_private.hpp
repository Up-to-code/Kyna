#pragma once

#include "kyna/hir/hir_program.hpp"
#include "kyna/mir/hir_lowering.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace kyna::mir_lowering_detail {

// Shared state for the lowering pass.  Expression and statement lowering are
// implemented in separate translation units, while this private contract
// keeps their compiler state explicit and out of the public API.
class HirLowerer {
public:
  explicit HirLowerer(const HirProgram &source);
  MirLoweringResult lower();

private:
  const HirProgram &hir;
  MirProgram mir;
  std::uint32_t *activeTemporaryCount{nullptr};
  std::vector<MirBasicBlock> *activeBlocks{nullptr};
  std::vector<MirExceptionRegion> *activeExceptionRegions{nullptr};
  MirBlockId currentBlock{0};
  std::unordered_map<std::uint32_t, MirTemporary> values;
  std::unordered_map<std::uint32_t, MirTemporary> locals;
  std::unordered_map<std::uint32_t, std::uint32_t> captureIndexes;

  struct LoopContext {
    std::string label;
    MirBlockId breakTarget;
    MirBlockId continueTarget;
  };
  std::vector<LoopContext> loops;
  std::vector<HirStatementId> cleanups;

  void activate(std::uint32_t &temporaryCount, std::vector<MirBasicBlock> &blocks,
                std::vector<MirExceptionRegion> &exceptionRegions);
  MirTemporary temporary();
  MirBasicBlock &current();
  MirBlockId addBlock();
  void terminate(MirTerminator::Node node, SourceSpan span);
  void finishFunction();
  bool lowerActiveCleanups();
  std::vector<MirBlockId> blocksFrom(std::size_t first) const;
  const LoopContext &loopTarget(const std::string &label) const;
  MirInstructionKind instructionFor(HirBinaryOperator operation) const;

  MirTemporary lowerExpression(HirExpressionId id);
  void lowerStatement(HirStatementId id);
};

} // namespace kyna::mir_lowering_detail
