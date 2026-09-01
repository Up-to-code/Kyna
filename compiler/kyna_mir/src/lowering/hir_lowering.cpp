#include "hir_lowering_private.hpp"
#include "kyna/mir/mir_verifier.hpp"

namespace kyna::mir_lowering_detail {

HirLowerer::HirLowerer(const HirProgram &source) : hir(source) {
  mir.name = source.name;
  for (const auto &sourceClass : source.classes) {
    MirClass target{sourceClass.name,
                    sourceClass.parent
                        ? std::optional<std::uint32_t>{sourceClass.parent->value}
                        : std::nullopt,
                    {}, {},
                    sourceClass.constructor
                        ? std::optional<std::uint32_t>{sourceClass.constructor->value}
                        : std::nullopt};
    for (const auto &field : sourceClass.fields)
      target.fields.push_back(field.name);
    for (const auto &method : sourceClass.methods)
      target.methods.push_back({method.name, method.function.value});
    mir.classes.push_back(std::move(target));
  }
  mir.blocks.emplace_back();
  activate(mir.temporaryCount, mir.blocks, mir.exceptionRegions);
}

MirLoweringResult HirLowerer::lower() {
  for (const auto statement : hir.body)
    lowerStatement(statement);
  finishFunction();

  for (const auto &sourceFunction : hir.functions) {
    mir.functions.push_back(
        {sourceFunction.name, static_cast<std::uint32_t>(sourceFunction.parameters.size()), 0,
         {}, {MirBasicBlock{}}, {}, sourceFunction.span, sourceFunction.captures});
    auto &target = mir.functions.back();
    activate(target.temporaryCount, target.blocks, target.exceptionRegions);
    for (std::size_t capture = 0; capture < sourceFunction.captures.size(); ++capture)
      captureIndexes.insert_or_assign(sourceFunction.captures[capture].value,
                                      static_cast<std::uint32_t>(capture));
    for (const auto parameter : sourceFunction.parameters)
      locals.insert_or_assign(parameter.value, temporary());
    lowerStatement(sourceFunction.body);
    finishFunction();
  }

  auto verification = verifyMir(mir);
  if (!verification.ok())
    return {std::nullopt, std::move(verification.diagnostics)};
  return {std::move(mir), {}};
}

void HirLowerer::activate(std::uint32_t &temporaryCount,
                          std::vector<MirBasicBlock> &blocks,
                          std::vector<MirExceptionRegion> &exceptionRegions) {
  activeTemporaryCount = &temporaryCount;
  activeBlocks = &blocks;
  activeExceptionRegions = &exceptionRegions;
  currentBlock = {};
  values.clear();
  locals.clear();
  captureIndexes.clear();
  loops.clear();
  cleanups.clear();
}

MirTemporary HirLowerer::temporary() { return MirTemporary{(*activeTemporaryCount)++}; }

MirBasicBlock &HirLowerer::current() { return activeBlocks->at(currentBlock.value); }

MirBlockId HirLowerer::addBlock() {
  const auto id = MirBlockId{static_cast<std::uint32_t>(activeBlocks->size())};
  activeBlocks->emplace_back();
  return id;
}

void HirLowerer::terminate(MirTerminator::Node node, SourceSpan span) {
  current().terminator = MirTerminator{std::move(node), span};
}

void HirLowerer::finishFunction() {
  if (current().terminator)
    return;
  const auto nullValue = temporary();
  current().instructions.push_back(
      {MirInstructionKind::Constant, nullValue, {}, {}, nullptr, {}, 0, {}});
  terminate(MirReturnTerminator{nullValue}, {});
}

const HirLowerer::LoopContext &HirLowerer::loopTarget(const std::string &label) const {
  if (label.empty())
    return loops.back();
  for (auto loop = loops.rbegin(); loop != loops.rend(); ++loop)
    if (loop->label == label)
      return *loop;
  return loops.back();
}

MirInstructionKind HirLowerer::instructionFor(HirBinaryOperator operation) const {
  switch (operation) {
  case HirBinaryOperator::Add: return MirInstructionKind::Add;
  case HirBinaryOperator::Subtract: return MirInstructionKind::Subtract;
  case HirBinaryOperator::Multiply: return MirInstructionKind::Multiply;
  case HirBinaryOperator::Divide: return MirInstructionKind::Divide;
  case HirBinaryOperator::Remainder: return MirInstructionKind::Remainder;
  case HirBinaryOperator::Equal: return MirInstructionKind::Equal;
  case HirBinaryOperator::NotEqual: return MirInstructionKind::NotEqual;
  case HirBinaryOperator::Less: return MirInstructionKind::Less;
  case HirBinaryOperator::LessEqual: return MirInstructionKind::LessEqual;
  case HirBinaryOperator::Greater: return MirInstructionKind::Greater;
  case HirBinaryOperator::GreaterEqual: return MirInstructionKind::GreaterEqual;
  case HirBinaryOperator::And:
  case HirBinaryOperator::Or: return MirInstructionKind::Move;
  }
  return MirInstructionKind::Move;
}

} // namespace kyna::mir_lowering_detail

namespace kyna {

MirLoweringResult lowerHirToMir(const HirProgram &program) {
  return mir_lowering_detail::HirLowerer(program).lower();
}

} // namespace kyna
