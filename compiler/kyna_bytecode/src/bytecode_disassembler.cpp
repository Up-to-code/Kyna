#include "kyna/bytecode/bytecode_disassembler.hpp"
#include <iomanip>
#include <sstream>

namespace kyna {

const char *opcodeName(OpCode opcode) {
  switch (opcode) {
#include "kyna/bytecode/opcode_names.inc"
  }
  return "invalid";
}

std::string disassembleBytecode(const BytecodeModule &module) {
  std::ostringstream output;
  output << "module " << module.name << " bytecode-v" << module.formatVersion << '\n';
  for (std::size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const auto &function = module.functions[functionIndex];
    output << "\nfunction " << functionIndex << ' ' << function.name << " registers="
           << function.registerCount << " parameters=" << function.parameterCount
           << " captures=" << function.captureCount << '\n';
    for (std::size_t offset = 0; offset < function.instructions.size(); ++offset) {
      const auto &instruction = function.instructions[offset];
      output << std::setw(4) << offset << "  " << std::left << std::setw(14)
             << opcodeName(instruction.opcode) << std::right;
      switch (instruction.opcode) {
      case OpCode::LoadConstant:
        output << " r" << instruction.destination << ", constant[" << instruction.first << ']';
        break;
      case OpCode::LoadNull:
        output << " r" << instruction.destination;
        break;
      case OpCode::LoadFunction:
        output << " r" << instruction.destination << ", function[" << instruction.first << ']';
        break;
      case OpCode::MakeClosure:
        output << " r" << instruction.destination << ", function[" << instruction.first
               << "] captures(";
        if (instruction.second < module.closureCaptures.size())
          for (std::size_t capture = 0;
               capture < module.closureCaptures[instruction.second].size(); ++capture) {
            if (capture) output << ", ";
            const auto &source = module.closureCaptures[instruction.second][capture];
            output << (source.kind == BytecodeCaptureSource::Kind::Local ? "r" : "capture[")
                   << source.index;
            if (source.kind == BytecodeCaptureSource::Kind::Capture) output << ']';
          }
        output << ')';
        break;
      case OpCode::LoadCapture:
        output << " r" << instruction.destination << ", capture[" << instruction.first << ']';
        break;
      case OpCode::StoreCapture:
        output << " capture[" << instruction.first << "], r" << instruction.second;
        break;
      case OpCode::Move:
        output << " r" << instruction.destination << ", r" << instruction.first;
        break;
      case OpCode::Negate:
      case OpCode::Not:
        output << " r" << instruction.destination << ", r" << instruction.first;
        break;
      case OpCode::Add:
      case OpCode::Subtract:
      case OpCode::Multiply:
      case OpCode::Divide:
      case OpCode::Remainder:
      case OpCode::Equal:
      case OpCode::NotEqual:
      case OpCode::Less:
      case OpCode::LessEqual:
      case OpCode::Greater:
      case OpCode::GreaterEqual:
        output << " r" << instruction.destination << ", r" << instruction.first << ", r"
               << instruction.second;
        break;
      case OpCode::Jump:
        output << ' ' << instruction.first;
        break;
      case OpCode::JumpIfFalse:
        output << " r" << instruction.first << ", " << instruction.second;
        break;
      case OpCode::Call:
        output << " r" << instruction.destination << ", function[" << instruction.first << "](";
        if (instruction.second < module.callArguments.size())
          for (std::size_t argument = 0;
               argument < module.callArguments[instruction.second].size(); ++argument) {
            if (argument) output << ", ";
            output << 'r' << module.callArguments[instruction.second][argument];
          }
        output << ')';
        break;
      case OpCode::CallIndirect:
        output << " r" << instruction.destination << ", r" << instruction.first << '(';
        if (instruction.second < module.callArguments.size())
          for (std::size_t argument = 0;
               argument < module.callArguments[instruction.second].size(); ++argument) {
            if (argument) output << ", ";
            output << 'r' << module.callArguments[instruction.second][argument];
          }
        output << ')';
        break;
      case OpCode::CallNative:
        output << " r" << instruction.destination << ", native[" << instruction.first << "](";
        if (instruction.second < module.callArguments.size())
          for (std::size_t argument = 0;
               argument < module.callArguments[instruction.second].size(); ++argument) {
            if (argument) output << ", ";
            output << 'r' << module.callArguments[instruction.second][argument];
          }
        output << ')';
        break;
      case OpCode::LoadMember:
        output << " r" << instruction.destination << ", r" << instruction.first
               << ", constant[" << instruction.second << ']';
        break;
      case OpCode::MakeArray:
      case OpCode::MakeObject:
        output << " r" << instruction.destination
               << (instruction.opcode == OpCode::MakeArray ? ", elements[" : ", fields[")
               << instruction.first << ']';
        if (instruction.opcode == OpCode::MakeObject)
          output << ", names[" << instruction.second << ']';
        break;
      case OpCode::LoadIndex:
        output << " r" << instruction.destination << ", r" << instruction.first << ", r"
               << instruction.second;
        break;
      case OpCode::Throw:
        output << " r" << instruction.first;
        break;
      case OpCode::Return:
        output << " r" << instruction.first;
        break;
      }
      if (instruction.span.known())
        output << "  ; " << instruction.span.line << ':' << instruction.span.column;
      output << '\n';
    }
    for (const auto &handler : function.exceptionHandlers)
      output << "       exception [" << handler.firstInstruction << ", "
             << handler.firstInstruction + handler.instructionCount << ") -> "
             << handler.handlerInstruction << " error=r" << handler.errorRegister << '\n';
  }
  return output.str();
}

} // namespace kyna
