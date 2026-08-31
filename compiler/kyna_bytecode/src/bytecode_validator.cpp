#include "kyna/bytecode/bytecode_validator.hpp"
#include <string>
#include <vector>

namespace kyna {
namespace {
void report(BytecodeValidationResult &result, std::string code, std::string message,
            SourceSpan span = {}) {
  result.diagnostics.emplace_back(std::move(message), span, false, std::move(code));
  result.diagnostics.back().category = "bytecode";
}

bool validRegister(std::uint32_t index, const BytecodeFunction &function) {
  return index < function.registerCount;
}
} // namespace

BytecodeValidationResult validateBytecode(const BytecodeModule &module) {
  BytecodeValidationResult result;
  if (module.formatVersion != BytecodeModule::FormatVersion)
    report(result, "KBC1001", "unsupported bytecode format version " +
                                  std::to_string(module.formatVersion));
  if (module.functions.empty()) {
    report(result, "KBC1002", "bytecode module has no functions");
    return result;
  }
  if (module.entryFunction >= module.functions.size())
    report(result, "KBC1003", "bytecode entry function is out of range");
  else if (module.functions[module.entryFunction].parameterCount != 0)
    report(result, "KBC1004", "bytecode entry function cannot require parameters");

  for (const auto &function : module.functions) {
    if (function.registerCount == 0)
      report(result, "KBC1101", "function '" + function.name + "' has no registers");
    if (function.instructions.empty()) {
      report(result, "KBC1102", "function '" + function.name + "' has no instructions");
      continue;
    }
    if (function.parameterCount > function.registerCount)
      report(result, "KBC1107", "function '" + function.name +
                                      "' has more parameters than registers");
    for (std::size_t offset = 0; offset < function.instructions.size(); ++offset) {
      const auto &instruction = function.instructions[offset];
      const auto registerError = [&](std::uint32_t index, const char *role) {
        if (!validRegister(index, function))
          report(result, "KBC1103", "instruction " + std::to_string(offset) + " in '" +
                                        function.name + "' uses an invalid " + role +
                                        " register",
                 instruction.span);
      };
      switch (instruction.opcode) {
      case OpCode::LoadConstant:
        registerError(instruction.destination, "destination");
        if (instruction.first >= module.constants.size())
          report(result, "KBC1104", "load-constant index is out of range", instruction.span);
        break;
      case OpCode::LoadNull:
        registerError(instruction.destination, "destination");
        break;
      case OpCode::LoadFunction:
        registerError(instruction.destination, "destination");
        if (instruction.first >= module.functions.size())
          report(result, "KBC1112", "function-reference index is out of range",
                 instruction.span);
        else if (instruction.first == module.entryFunction)
          report(result, "KBC1113", "module entry function cannot be used as a value",
                 instruction.span);
        break;
      case OpCode::MakeClosure:
        registerError(instruction.destination, "closure destination");
        if (instruction.first >= module.functions.size()) {
          report(result, "KBC1114", "closure function index is out of range", instruction.span);
          break;
        }
        if (instruction.first == module.entryFunction)
          report(result, "KBC1115", "module entry function cannot be closed over",
                 instruction.span);
        if (instruction.second >= module.closureCaptures.size()) {
          report(result, "KBC1116", "closure capture-list index is out of range",
                 instruction.span);
          break;
        }
        if (module.closureCaptures[instruction.second].size() !=
            module.functions[instruction.first].captureCount)
          report(result, "KBC1117", "closure capture count does not match function '" +
                                         module.functions[instruction.first].name + "'",
                 instruction.span);
        for (const auto &capture : module.closureCaptures[instruction.second]) {
          if (capture.kind == BytecodeCaptureSource::Kind::Local)
            registerError(capture.index, "captured local");
          else if (capture.index >= function.captureCount)
            report(result, "KBC1118", "parent capture index is out of range", instruction.span);
        }
        break;
      case OpCode::LoadCapture:
        registerError(instruction.destination, "capture destination");
        if (instruction.first >= function.captureCount)
          report(result, "KBC1119", "capture load index is out of range", instruction.span);
        break;
      case OpCode::StoreCapture:
        registerError(instruction.destination, "capture result");
        registerError(instruction.second, "capture source");
        if (instruction.first >= function.captureCount)
          report(result, "KBC1120", "capture store index is out of range", instruction.span);
        break;
      case OpCode::Move:
        registerError(instruction.destination, "destination");
        registerError(instruction.first, "source");
        break;
      case OpCode::Negate:
      case OpCode::Not:
        registerError(instruction.destination, "destination");
        registerError(instruction.first, "operand");
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
        registerError(instruction.destination, "destination");
        registerError(instruction.first, "left operand");
        registerError(instruction.second, "right operand");
        break;
      case OpCode::Jump:
        if (instruction.first >= function.instructions.size())
          report(result, "KBC1105", "jump target is out of range", instruction.span);
        break;
      case OpCode::JumpIfFalse:
        registerError(instruction.first, "condition");
        if (instruction.second >= function.instructions.size())
          report(result, "KBC1105", "conditional jump target is out of range", instruction.span);
        break;
      case OpCode::Call:
        registerError(instruction.destination, "call destination");
        if (instruction.first >= module.functions.size()) {
          report(result, "KBC1108", "call function index is out of range", instruction.span);
          break;
        }
        if (instruction.first == module.entryFunction) {
          report(result, "KBC1111", "module entry function cannot be called", instruction.span);
          break;
        }
        if (instruction.second >= module.callArguments.size()) {
          report(result, "KBC1109", "call argument-list index is out of range",
                 instruction.span);
          break;
        }
        if (module.callArguments[instruction.second].size() !=
            module.functions[instruction.first].parameterCount)
          report(result, "KBC1110", "call argument count does not match function '" +
                                         module.functions[instruction.first].name + "'",
                 instruction.span);
        for (const auto argument : module.callArguments[instruction.second])
          registerError(argument, "call argument");
        break;
      case OpCode::CallIndirect:
        registerError(instruction.destination, "call destination");
        registerError(instruction.first, "callee");
        if (instruction.second >= module.callArguments.size()) {
          report(result, "KBC1109", "indirect-call argument-list index is out of range",
                 instruction.span);
          break;
        }
        for (const auto argument : module.callArguments[instruction.second])
          registerError(argument, "indirect-call argument");
        break;
      case OpCode::CallNative:
        registerError(instruction.destination, "native-call destination");
        if (instruction.first >= module.nativeFunctions.size())
          report(result, "KBC1125", "native function index is out of range", instruction.span);
        if (instruction.second >= module.callArguments.size()) {
          report(result, "KBC1109", "native-call argument-list index is out of range",
                 instruction.span);
          break;
        }
        for (const auto argument : module.callArguments[instruction.second])
          registerError(argument, "native-call argument");
        break;
      case OpCode::LoadMember:
        registerError(instruction.destination, "member destination");
        registerError(instruction.first, "member object");
        if (instruction.second >= module.constants.size() ||
            !std::holds_alternative<std::string>(module.constants[instruction.second]))
          report(result, "KBC1124", "member name is not a valid string constant",
                 instruction.span);
        break;
      case OpCode::MakeArray:
      case OpCode::MakeObject:
        registerError(instruction.destination, "collection destination");
        if (instruction.first >= module.callArguments.size()) {
          report(result, "KBC1126", "collection element-list index is out of range",
                 instruction.span);
          break;
        }
        for (const auto element : module.callArguments[instruction.first])
          registerError(element, "collection element");
        if (instruction.opcode == OpCode::MakeObject &&
            (instruction.second >= module.objectFieldNames.size() ||
             module.objectFieldNames[instruction.second].size() !=
                 module.callArguments[instruction.first].size()))
          report(result, "KBC1127", "object field names do not match object values",
                 instruction.span);
        break;
      case OpCode::MakeInstance:
        registerError(instruction.destination, "instance destination");
        if (instruction.first >= module.classes.size())
          report(result, "KBC1201", "instance class index is out of range", instruction.span);
        if (instruction.second >= module.callArguments.size()) {
          report(result, "KBC1202", "constructor argument-list index is out of range",
                 instruction.span);
          break;
        }
        for (const auto argument : module.callArguments[instruction.second])
          registerError(argument, "constructor argument");
        break;
      case OpCode::LoadIndex:
        registerError(instruction.destination, "index destination");
        registerError(instruction.first, "indexed value");
        registerError(instruction.second, "index");
        break;
      case OpCode::StoreIndex:
      case OpCode::StoreMember: {
        registerError(instruction.destination, "mutation result");
        if (instruction.first >= module.callArguments.size()) {
          report(result, "KBC1128", "mutation operand-list index is out of range",
                 instruction.span);
          break;
        }
        const auto expected = instruction.opcode == OpCode::StoreIndex ? 3u : 2u;
        if (module.callArguments[instruction.first].size() != expected)
          report(result, "KBC1129", "mutation operand count is invalid", instruction.span);
        for (const auto operand : module.callArguments[instruction.first])
          registerError(operand, "mutation operand");
        if (instruction.opcode == OpCode::StoreMember &&
            (instruction.second >= module.constants.size() ||
             !std::holds_alternative<std::string>(module.constants[instruction.second])))
          report(result, "KBC1130", "member store name is not a string constant",
                 instruction.span);
        break;
      }
      case OpCode::Throw:
        registerError(instruction.first, "thrown value");
        break;
      case OpCode::Return:
        registerError(instruction.first, "return value");
        break;
      }
    }
    for (const auto &handler : function.exceptionHandlers) {
      if (handler.instructionCount == 0 ||
          handler.firstInstruction >= function.instructions.size() ||
          handler.instructionCount > function.instructions.size() - handler.firstInstruction)
        report(result, "KBC1121", "exception handler protects an invalid instruction range");
      if (handler.handlerInstruction >= function.instructions.size())
        report(result, "KBC1122", "exception handler target is out of range");
      if (!validRegister(handler.errorRegister, function))
        report(result, "KBC1123", "exception handler error register is out of range");
    }
    const auto finalOpcode = function.instructions.back().opcode;
    if (finalOpcode != OpCode::Return && finalOpcode != OpCode::Jump &&
        finalOpcode != OpCode::Throw)
      report(result, "KBC1106",
             "function '" + function.name + "' can fall past its final instruction",
             function.instructions.back().span);
  }
  for (const auto &klass : module.classes) {
    if (klass.parent && *klass.parent >= module.classes.size())
      report(result, "KBC1203", "class '" + klass.name + "' has an invalid parent");
    if (klass.constructor && *klass.constructor >= module.functions.size())
      report(result, "KBC1204", "class '" + klass.name + "' has an invalid constructor");
    else if (klass.constructor && module.functions[*klass.constructor].parameterCount == 0)
      report(result, "KBC1206", "constructor for class '" + klass.name +
                                      "' does not accept the implicit receiver");
    for (const auto &method : klass.methods) {
      if (method.function >= module.functions.size())
        report(result, "KBC1205", "class '" + klass.name + "' has an invalid method '" +
                                      method.name + "'");
      else if (module.functions[method.function].parameterCount == 0)
        report(result, "KBC1207", "method '" + klass.name + "." + method.name +
                                      "' does not accept the implicit receiver");
    }
  }
  for (std::size_t classIndex = 0; classIndex < module.classes.size(); ++classIndex) {
    std::vector<bool> visited(module.classes.size(), false);
    std::optional<std::uint32_t> cursor{static_cast<std::uint32_t>(classIndex)};
    while (cursor && *cursor < module.classes.size()) {
      if (visited[*cursor]) {
        report(result, "KBC1208", "class inheritance metadata contains a cycle involving '" +
                                        module.classes[*cursor].name + "'");
        break;
      }
      visited[*cursor] = true;
      cursor = module.classes[*cursor].parent;
    }
  }
  return result;
}

} // namespace kyna
