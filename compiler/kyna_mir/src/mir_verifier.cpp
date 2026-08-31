#include "kyna/mir/mir_verifier.hpp"
#include <type_traits>

namespace kyna {
namespace {
void addError(std::vector<Diagnostic> &diagnostics, std::string message, SourceSpan span,
              std::string code) {
  Diagnostic diagnostic{std::move(message), span, false, std::move(code)};
  diagnostic.category = "mir";
  diagnostic.help = "this indicates an internal compiler defect; please report a reproducible case";
  diagnostics.push_back(std::move(diagnostic));
}

std::uint32_t parameterCount(const MirProgram &program, std::uint32_t function) {
  return function == 0 ? 0 : program.functions.at(function - 1).parameterCount;
}

void verifyBody(const MirProgram &program, std::string_view name, std::uint32_t parameters,
                std::uint32_t temporaryCount, std::uint32_t captureCount, MirBlockId entryBlock,
                const std::vector<MirBasicBlock> &blocks,
                const std::vector<MirExceptionRegion> &exceptionRegions,
                std::vector<Diagnostic> &diagnostics) {
  if (blocks.empty() || entryBlock.value >= blocks.size())
    addError(diagnostics, "MIR entry block is missing or invalid in '" + std::string(name) + "'",
             {}, "KMIR1101");
  if (parameters > temporaryCount)
    addError(diagnostics, "MIR function '" + std::string(name) +
                              "' has more parameters than temporaries",
             {}, "KMIR1110");
  const auto validTemporary = [&](MirTemporary temporary) {
    return temporary.value < temporaryCount;
  };
  for (std::size_t index = 0; index < blocks.size(); ++index) {
    const auto &block = blocks[index];
    for (const auto &instruction : block.instructions) {
      if (!validTemporary(instruction.destination))
        addError(diagnostics, "MIR instruction writes an invalid temporary", instruction.span,
                 "KMIR1102");
      const bool unary = instruction.kind == MirInstructionKind::Negate ||
                         instruction.kind == MirInstructionKind::Not;
      if ((instruction.kind == MirInstructionKind::Move || unary) &&
          !validTemporary(instruction.first))
        addError(diagnostics, "MIR unary or move instruction reads an invalid temporary",
                 instruction.span,
                 "KMIR1103");
      if (instruction.kind != MirInstructionKind::Constant &&
          instruction.kind != MirInstructionKind::FunctionReference &&
          instruction.kind != MirInstructionKind::Closure &&
          instruction.kind != MirInstructionKind::LoadCapture &&
          instruction.kind != MirInstructionKind::StoreCapture &&
          instruction.kind != MirInstructionKind::Move &&
          !unary &&
          instruction.kind != MirInstructionKind::Call &&
          instruction.kind != MirInstructionKind::CallIndirect &&
          instruction.kind != MirInstructionKind::CallNative &&
          instruction.kind != MirInstructionKind::LoadMember) {
        if (!validTemporary(instruction.first))
          addError(diagnostics, "MIR instruction reads an invalid temporary", instruction.span,
                   "KMIR1103");
        if (!validTemporary(instruction.second))
          addError(diagnostics, "MIR instruction reads an invalid second temporary",
                   instruction.span, "KMIR1104");
      }
      if (instruction.kind == MirInstructionKind::LoadMember) {
        if (!validTemporary(instruction.first))
          addError(diagnostics, "MIR member load reads an invalid object temporary",
                   instruction.span, "KMIR1129");
        if (!std::holds_alternative<std::string>(instruction.constant))
          addError(diagnostics, "MIR member load has a non-string member name",
                   instruction.span, "KMIR1130");
      }
      if (instruction.kind == MirInstructionKind::CallNative) {
        if (!std::holds_alternative<std::string>(instruction.constant))
          addError(diagnostics, "MIR native call has a non-string function name",
                   instruction.span, "KMIR1131");
        for (const auto argument : instruction.arguments)
          if (!validTemporary(argument))
            addError(diagnostics, "MIR native call reads an invalid argument temporary",
                     instruction.span, "KMIR1132");
      }
      if (instruction.kind == MirInstructionKind::Call) {
        if (instruction.function == 0 || instruction.function >= program.functions.size() + 1)
          addError(diagnostics, "MIR call targets an invalid function", instruction.span,
                   "KMIR1111");
        else if (instruction.arguments.size() != parameterCount(program, instruction.function))
          addError(diagnostics, "MIR call argument count does not match its function",
                   instruction.span, "KMIR1112");
        for (const auto argument : instruction.arguments)
          if (!validTemporary(argument))
            addError(diagnostics, "MIR call reads an invalid argument temporary",
                     instruction.span, "KMIR1113");
      }
      if (instruction.kind == MirInstructionKind::FunctionReference &&
          (instruction.function == 0 || instruction.function >= program.functions.size() + 1))
        addError(diagnostics, "MIR function reference targets an invalid function",
                 instruction.span, "KMIR1114");
      if (instruction.kind == MirInstructionKind::LoadCapture &&
          instruction.capture >= captureCount)
        addError(diagnostics, "MIR capture load index is out of range", instruction.span,
                 "KMIR1117");
      if (instruction.kind == MirInstructionKind::StoreCapture) {
        if (instruction.capture >= captureCount)
          addError(diagnostics, "MIR capture store index is out of range", instruction.span,
                   "KMIR1118");
        if (!validTemporary(instruction.first))
          addError(diagnostics, "MIR capture store reads an invalid temporary",
                   instruction.span, "KMIR1119");
      }
      if (instruction.kind == MirInstructionKind::Closure) {
        if (instruction.function == 0 || instruction.function >= program.functions.size() + 1) {
          addError(diagnostics, "MIR closure targets an invalid function", instruction.span,
                   "KMIR1120");
        } else if (instruction.captureSources.size() !=
                   program.functions[instruction.function - 1].captures.size()) {
          addError(diagnostics, "MIR closure capture count does not match its function",
                   instruction.span, "KMIR1121");
        }
        for (const auto &source : instruction.captureSources) {
          if (source.kind == MirCaptureSource::Kind::Local && source.index >= temporaryCount)
            addError(diagnostics, "MIR closure captures an invalid local temporary",
                     instruction.span, "KMIR1122");
          if (source.kind == MirCaptureSource::Kind::Capture && source.index >= captureCount)
            addError(diagnostics, "MIR closure captures an invalid parent capture",
                     instruction.span, "KMIR1123");
        }
      }
      if (instruction.kind == MirInstructionKind::CallIndirect) {
        if (!validTemporary(instruction.first))
          addError(diagnostics, "MIR indirect call reads an invalid callee temporary",
                   instruction.span, "KMIR1115");
        for (const auto argument : instruction.arguments)
          if (!validTemporary(argument))
            addError(diagnostics, "MIR indirect call reads an invalid argument temporary",
                     instruction.span, "KMIR1116");
      }
    }
    if (!block.terminator) {
      addError(diagnostics, "MIR basic block bb" + std::to_string(index) + " in '" +
                                std::string(name) + "' has no terminator",
               {}, "KMIR1105");
      continue;
    }
    std::visit(
        [&](const auto &terminator) {
          using T = std::decay_t<decltype(terminator)>;
          if constexpr (std::is_same_v<T, MirReturnTerminator>) {
            if (!validTemporary(terminator.value))
              addError(diagnostics, "MIR return reads an invalid temporary",
                       block.terminator->span, "KMIR1106");
          } else if constexpr (std::is_same_v<T, MirGotoTerminator>) {
            if (terminator.target.value >= blocks.size())
              addError(diagnostics, "MIR goto targets an invalid block", block.terminator->span,
                       "KMIR1107");
          } else if constexpr (std::is_same_v<T, MirBranchTerminator>) {
            if (!validTemporary(terminator.condition))
              addError(diagnostics, "MIR branch reads an invalid condition",
                       block.terminator->span, "KMIR1108");
            if (terminator.trueBlock.value >= blocks.size() ||
                terminator.falseBlock.value >= blocks.size())
              addError(diagnostics, "MIR branch targets an invalid block", block.terminator->span,
                       "KMIR1109");
          } else if (!validTemporary(terminator.value)) {
            addError(diagnostics, "MIR throw reads an invalid temporary",
                     block.terminator->span, "KMIR1124");
          }
        },
        block.terminator->node);
  }
  for (const auto &region : exceptionRegions) {
    if (region.protectedBlocks.empty())
      addError(diagnostics, "MIR exception region protects no blocks", {}, "KMIR1125");
    for (const auto block : region.protectedBlocks)
      if (block.value >= blocks.size())
        addError(diagnostics, "MIR exception region references an invalid protected block", {},
                 "KMIR1126");
    if (region.handler.value >= blocks.size())
      addError(diagnostics, "MIR exception handler targets an invalid block", {}, "KMIR1127");
    if (!validTemporary(region.errorDestination))
      addError(diagnostics, "MIR exception handler writes an invalid temporary", {},
               "KMIR1128");
  }
}
} // namespace

MirVerificationResult verifyMir(const MirProgram &program) {
  std::vector<Diagnostic> diagnostics;
  verifyBody(program, "<module>", 0, program.temporaryCount, 0, program.entryBlock,
             program.blocks, program.exceptionRegions, diagnostics);
  for (const auto &function : program.functions)
    verifyBody(program, function.name, function.parameterCount, function.temporaryCount,
               static_cast<std::uint32_t>(function.captures.size()), function.entryBlock,
               function.blocks, function.exceptionRegions, diagnostics);
  return {std::move(diagnostics)};
}

} // namespace kyna
