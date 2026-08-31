#include "kyna/bytecode/program_bytecode_compiler.hpp"
#include "kyna/bytecode/bytecode_validator.hpp"
#include "kyna/mir/mir_verifier.hpp"
#include <type_traits>

namespace kyna {
namespace {

OpCode opcodeFor(MirInstructionKind kind) {
  switch (kind) {
  case MirInstructionKind::Move: return OpCode::Move;
  case MirInstructionKind::FunctionReference: return OpCode::LoadFunction;
  case MirInstructionKind::Closure: return OpCode::MakeClosure;
  case MirInstructionKind::LoadCapture: return OpCode::LoadCapture;
  case MirInstructionKind::StoreCapture: return OpCode::StoreCapture;
  case MirInstructionKind::Negate: return OpCode::Negate;
  case MirInstructionKind::Not: return OpCode::Not;
  case MirInstructionKind::Add: return OpCode::Add;
  case MirInstructionKind::Subtract: return OpCode::Subtract;
  case MirInstructionKind::Multiply: return OpCode::Multiply;
  case MirInstructionKind::Divide: return OpCode::Divide;
  case MirInstructionKind::Remainder: return OpCode::Remainder;
  case MirInstructionKind::Equal: return OpCode::Equal;
  case MirInstructionKind::NotEqual: return OpCode::NotEqual;
  case MirInstructionKind::Less: return OpCode::Less;
  case MirInstructionKind::LessEqual: return OpCode::LessEqual;
  case MirInstructionKind::Greater: return OpCode::Greater;
  case MirInstructionKind::GreaterEqual: return OpCode::GreaterEqual;
  case MirInstructionKind::Constant: return OpCode::LoadConstant;
  case MirInstructionKind::Call: return OpCode::Call;
  case MirInstructionKind::CallIndirect: return OpCode::CallIndirect;
  case MirInstructionKind::LoadMember: return OpCode::LoadMember;
  }
  return OpCode::LoadNull;
}

std::size_t terminatorSize(const MirTerminator &terminator) {
  return std::holds_alternative<MirBranchTerminator>(terminator.node) ? 2 : 1;
}

void compileBody(BytecodeModule &module, BytecodeFunction &function,
                 const std::vector<MirBasicBlock> &blocks,
                 const std::vector<MirExceptionRegion> &exceptionRegions) {
  std::vector<std::uint32_t> blockOffsets(blocks.size());
  std::size_t offset = 0;
  for (std::size_t index = 0; index < blocks.size(); ++index) {
    blockOffsets[index] = static_cast<std::uint32_t>(offset);
    offset += blocks[index].instructions.size();
    offset += terminatorSize(*blocks[index].terminator);
  }

  for (const auto &block : blocks) {
    for (const auto &instruction : block.instructions) {
      if (instruction.kind == MirInstructionKind::Constant) {
        module.constants.push_back(instruction.constant);
        function.instructions.push_back(
            {OpCode::LoadConstant, instruction.destination.value,
             static_cast<std::uint32_t>(module.constants.size() - 1), 0, instruction.span});
      } else if (instruction.kind == MirInstructionKind::FunctionReference) {
        function.instructions.push_back(
            {OpCode::LoadFunction, instruction.destination.value, instruction.function, 0,
             instruction.span});
      } else if (instruction.kind == MirInstructionKind::Closure) {
        std::vector<BytecodeCaptureSource> captures;
        captures.reserve(instruction.captureSources.size());
        for (const auto &source : instruction.captureSources)
          captures.push_back(
              {source.kind == MirCaptureSource::Kind::Local
                   ? BytecodeCaptureSource::Kind::Local
                   : BytecodeCaptureSource::Kind::Capture,
               source.index});
        module.closureCaptures.push_back(std::move(captures));
        function.instructions.push_back(
            {OpCode::MakeClosure, instruction.destination.value, instruction.function,
             static_cast<std::uint32_t>(module.closureCaptures.size() - 1), instruction.span});
      } else if (instruction.kind == MirInstructionKind::LoadCapture) {
        function.instructions.push_back({OpCode::LoadCapture, instruction.destination.value,
                                         instruction.capture, 0, instruction.span});
      } else if (instruction.kind == MirInstructionKind::StoreCapture) {
        function.instructions.push_back({OpCode::StoreCapture, instruction.destination.value,
                                         instruction.capture, instruction.first.value,
                                         instruction.span});
      } else if (instruction.kind == MirInstructionKind::Call ||
                 instruction.kind == MirInstructionKind::CallIndirect) {
        std::vector<std::uint32_t> arguments;
        arguments.reserve(instruction.arguments.size());
        for (const auto argument : instruction.arguments)
          arguments.push_back(argument.value);
        module.callArguments.push_back(std::move(arguments));
        function.instructions.push_back({instruction.kind == MirInstructionKind::Call
                                             ? OpCode::Call
                                             : OpCode::CallIndirect,
                                         instruction.destination.value,
                                         instruction.kind == MirInstructionKind::Call
                                             ? instruction.function
                                             : instruction.first.value,
                                         static_cast<std::uint32_t>(module.callArguments.size() - 1),
                                         instruction.span});
      } else if (instruction.kind == MirInstructionKind::LoadMember) {
        module.constants.push_back(instruction.constant);
        function.instructions.push_back(
            {OpCode::LoadMember, instruction.destination.value, instruction.first.value,
             static_cast<std::uint32_t>(module.constants.size() - 1), instruction.span});
      } else {
        function.instructions.push_back(
            {opcodeFor(instruction.kind), instruction.destination.value, instruction.first.value,
             instruction.second.value, instruction.span});
      }
    }
    std::visit(
        [&](const auto &terminator) {
          using T = std::decay_t<decltype(terminator)>;
          const auto span = block.terminator->span;
          if constexpr (std::is_same_v<T, MirReturnTerminator>)
            function.instructions.push_back({OpCode::Return, 0, terminator.value.value, 0, span});
          else if constexpr (std::is_same_v<T, MirGotoTerminator>)
            function.instructions.push_back(
                {OpCode::Jump, 0, blockOffsets[terminator.target.value], 0, span});
          else if constexpr (std::is_same_v<T, MirBranchTerminator>) {
            function.instructions.push_back(
                {OpCode::JumpIfFalse, 0, terminator.condition.value,
                 blockOffsets[terminator.falseBlock.value], span});
            function.instructions.push_back(
                {OpCode::Jump, 0, blockOffsets[terminator.trueBlock.value], 0, span});
          } else
            function.instructions.push_back({OpCode::Throw, 0, terminator.value.value, 0, span});
        },
        block.terminator->node);
  }
  for (const auto &region : exceptionRegions)
    for (const auto block : region.protectedBlocks) {
      const auto begin = blockOffsets[block.value];
      const auto end = block.value + 1 < blockOffsets.size()
                           ? blockOffsets[block.value + 1]
                           : static_cast<std::uint32_t>(function.instructions.size());
      function.exceptionHandlers.push_back(
          {begin, end - begin, blockOffsets[region.handler.value],
           region.errorDestination.value});
    }
}

} // namespace

BytecodeCompileResult compileMirToBytecode(const MirProgram &program) {
  auto mirValidation = verifyMir(program);
  if (!mirValidation.ok())
    return {std::nullopt, std::move(mirValidation.diagnostics)};

  BytecodeModule module;
  module.name = program.name;
  module.functions.reserve(program.functions.size() + 1);
  module.functions.push_back({"<module>", program.temporaryCount, {}, 0, 0, {}});
  for (const auto &function : program.functions)
    module.functions.push_back(
        {function.name, function.temporaryCount, {}, function.parameterCount,
         static_cast<std::uint32_t>(function.captures.size()), {}});

  compileBody(module, module.functions.front(), program.blocks, program.exceptionRegions);
  for (std::size_t index = 0; index < program.functions.size(); ++index)
    compileBody(module, module.functions[index + 1], program.functions[index].blocks,
                program.functions[index].exceptionRegions);

  auto validation = validateBytecode(module);
  if (!validation.ok())
    return {std::nullopt, std::move(validation.diagnostics)};
  return {std::move(module), {}};
}

} // namespace kyna
