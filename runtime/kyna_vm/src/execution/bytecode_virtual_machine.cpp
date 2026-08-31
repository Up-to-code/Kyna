#include "kyna/execution/bytecode_virtual_machine.hpp"
#include "kyna/bytecode/bytecode_validator.hpp"
#include "kyna/memory/tracing_heap.hpp"
#include <cmath>
#include <limits>
#include <optional>

namespace kyna {
namespace {
RuntimeValue fromConstant(const BytecodeConstant &constant) {
  return std::visit([](const auto &value) { return RuntimeValue(value); }, constant);
}

struct CallFrame {
  std::uint32_t function{0};
  std::vector<RuntimeValue> registers;
  std::size_t instructionPointer{0};
  std::optional<std::uint32_t> returnDestination;
  SourceSpan callSite;
  std::vector<VmCaptureCell *> captures;
  std::vector<VmCaptureCell *> registerCells;
};

Diagnostic runtimeDiagnostic(std::string code, std::string message, SourceSpan span,
                             const BytecodeModule &module,
                             const std::vector<CallFrame> &frames) {
  Diagnostic diagnostic{std::move(message), span, false, std::move(code)};
  diagnostic.category = "runtime";
  for (std::size_t offset = 0; offset < frames.size(); ++offset) {
    const auto index = frames.size() - 1 - offset;
    const auto frameSpan = index + 1 == frames.size() ? span : frames[index + 1].callSite;
    diagnostic.callFrames.push_back({module.functions[frames[index].function].name, frameSpan});
  }
  return diagnostic;
}

bool number(const RuntimeValue &value, double &result, bool &integer) {
  if (const auto item = std::get_if<std::int64_t>(&value.data)) {
    result = static_cast<double>(*item);
    integer = true;
    return true;
  }
  if (const auto item = std::get_if<double>(&value.data)) {
    result = *item;
    integer = false;
    return true;
  }
  return false;
}

bool checkedIntegerArithmetic(OpCode opcode, std::int64_t left, std::int64_t right,
                              std::int64_t &result) {
  const auto minimum = std::numeric_limits<std::int64_t>::min();
  const auto maximum = std::numeric_limits<std::int64_t>::max();
  switch (opcode) {
  case OpCode::Add:
    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right))
      return false;
    result = left + right;
    return true;
  case OpCode::Subtract:
    if ((right < 0 && left > maximum + right) || (right > 0 && left < minimum + right))
      return false;
    result = left - right;
    return true;
  case OpCode::Multiply:
    if (left == 0 || right == 0) {
      result = 0;
      return true;
    }
    if ((left == -1 && right == minimum) || (right == -1 && left == minimum))
      return false;
    if (left > 0) {
      if ((right > 0 && left > maximum / right) ||
          (right < 0 && right < minimum / left))
        return false;
    } else if ((right > 0 && left < minimum / right) ||
               (right < 0 && left < maximum / right))
      return false;
    result = left * right;
    return true;
  default:
    return false;
  }
}
} // namespace

BytecodeExecutionResult
BytecodeVirtualMachine::execute(const BytecodeModule &module,
                                BytecodeNativeAdapter *nativeAdapter) const {
  auto validation = validateBytecode(module);
  if (!validation.ok())
    return {{}, std::move(validation.diagnostics)};

  constexpr std::size_t MaximumCallFrames = 4096;
  Heap heap;
  std::vector<CallFrame> frames;
  const auto &entry = module.functions[module.entryFunction];
  frames.push_back(
      {module.entryFunction, std::vector<RuntimeValue>(entry.registerCount), 0, std::nullopt, {},
       {}, std::vector<VmCaptureCell *>(entry.registerCount, nullptr)});

  const auto readRegister = [](const CallFrame &frame, std::uint32_t index)
      -> const RuntimeValue & {
    return frame.registerCells[index] ? frame.registerCells[index]->value
                                      : frame.registers[index];
  };
  const auto writeRegister = [](CallFrame &frame, std::uint32_t index, RuntimeValue value) {
    frame.registers[index] = value;
    if (frame.registerCells[index])
      frame.registerCells[index]->value = std::move(value);
  };
  const auto collectAtSafepoint = [&] {
    HeapRoots roots;
    for (const auto &activeFrame : frames) {
      for (const auto &value : activeFrame.registers)
        roots.values.push_back(&value);
      for (auto *cell : activeFrame.registerCells)
        if (cell)
          roots.captureCells.push_back(cell);
      for (auto *cell : activeFrame.captures)
        if (cell)
          roots.captureCells.push_back(cell);
    }
    heap.maybeCollectRoots(roots);
  };
  const auto unwindError = [&](ErrorObject *error, SourceSpan span)
      -> std::optional<BytecodeExecutionResult> {
    std::optional<std::size_t> handledFrame;
    const BytecodeFunction::ExceptionHandler *matched = nullptr;
    std::size_t faultInstruction = frames.back().instructionPointer;
    for (std::size_t count = frames.size(); count > 0; --count) {
      const auto frameIndex = count - 1;
      const auto &candidateFunction = module.functions[frames[frameIndex].function];
      for (const auto &handler : candidateFunction.exceptionHandlers)
        if (faultInstruction >= handler.firstInstruction &&
            faultInstruction - handler.firstInstruction < handler.instructionCount) {
          handledFrame = frameIndex;
          matched = &handler;
          break;
        }
      if (handledFrame)
        break;
      if (frameIndex > 0)
        faultInstruction = frames[frameIndex - 1].instructionPointer - 1;
    }

    if (!handledFrame) {
      auto diagnostic = runtimeDiagnostic(error->code.empty() ? "KVM2301" : error->code,
                                          error->message, span, module, frames);
      if (!std::holds_alternative<std::nullptr_t>(error->cause.data))
        diagnostic.notes.push_back("cause: " + error->cause.display());
      diagnostic.help = "catch this error with 'try/catch' or fix the failing operation";
      return BytecodeExecutionResult{{}, {std::move(diagnostic)}, heap.stats()};
    }

    frames.resize(*handledFrame + 1);
    auto &handled = frames.back();
    writeRegister(handled, matched->errorRegister, RuntimeValue(error));
    handled.instructionPointer = matched->handlerInstruction;
    collectAtSafepoint();
    return std::nullopt;
  };
  const auto raise = [&](std::string code, std::string message, SourceSpan span,
                         RuntimeValue cause = RuntimeValue()) {
    return unwindError(heap.allocateError(std::move(message), std::move(code), cause), span);
  };

  while (!frames.empty()) {
    auto &frame = frames.back();
    const auto &function = module.functions[frame.function];
    if (frame.instructionPointer >= function.instructions.size())
      return {{}, {runtimeDiagnostic("KVM2099", "instruction pointer escaped function '" +
                                                   function.name + "'",
                                     function.instructions.back().span, module, frames)}};

    const auto &instruction = function.instructions[frame.instructionPointer];
    switch (instruction.opcode) {
    case OpCode::LoadConstant:
      writeRegister(frame, instruction.destination,
                    fromConstant(module.constants[instruction.first]));
      break;
    case OpCode::LoadNull:
      writeRegister(frame, instruction.destination, RuntimeValue());
      break;
    case OpCode::LoadFunction:
      writeRegister(frame, instruction.destination, VmFunctionReference{instruction.first});
      break;
    case OpCode::MakeClosure: {
      std::vector<VmCaptureCell *> captures;
      for (const auto &source : module.closureCaptures[instruction.second]) {
        if (source.kind == BytecodeCaptureSource::Kind::Capture) {
          captures.push_back(frame.captures[source.index]);
          continue;
        }
        auto *&cell = frame.registerCells[source.index];
        if (!cell)
          cell = heap.allocateCaptureCell(readRegister(frame, source.index));
        captures.push_back(cell);
      }
      writeRegister(frame, instruction.destination,
                    heap.allocateClosure(instruction.first, std::move(captures)));
      collectAtSafepoint();
      break;
    }
    case OpCode::LoadCapture:
      writeRegister(frame, instruction.destination, frame.captures[instruction.first]->value);
      break;
    case OpCode::StoreCapture:
      frame.captures[instruction.first]->value = readRegister(frame, instruction.second);
      writeRegister(frame, instruction.destination, readRegister(frame, instruction.second));
      break;
    case OpCode::Move:
      writeRegister(frame, instruction.destination, readRegister(frame, instruction.first));
      break;
    case OpCode::Not:
      writeRegister(frame, instruction.destination,
                    RuntimeValue(!readRegister(frame, instruction.first).isTruthy()));
      break;
    case OpCode::Negate:
      if (const auto integer =
              std::get_if<std::int64_t>(&readRegister(frame, instruction.first).data)) {
        if (*integer == std::numeric_limits<std::int64_t>::min()) {
          if (auto failure = raise("KVM2005", "integer overflow while negating value",
                                   instruction.span))
            return *std::move(failure);
          continue;
        }
        writeRegister(frame, instruction.destination, RuntimeValue(-*integer));
      } else if (const auto floating =
                     std::get_if<double>(&readRegister(frame, instruction.first).data))
        writeRegister(frame, instruction.destination, RuntimeValue(-*floating));
      else {
        if (auto failure = raise("KVM2002", "negate requires a numeric operand",
                                 instruction.span))
          return *std::move(failure);
        continue;
      }
      break;
    case OpCode::Equal:
      writeRegister(frame, instruction.destination,
                    RuntimeValue(readRegister(frame, instruction.first)
                                     .equals(readRegister(frame, instruction.second))));
      break;
    case OpCode::NotEqual:
      writeRegister(frame, instruction.destination,
                    RuntimeValue(!readRegister(frame, instruction.first)
                                      .equals(readRegister(frame, instruction.second))));
      break;
    case OpCode::Add:
      if (std::holds_alternative<std::string>(readRegister(frame, instruction.first).data) ||
          std::holds_alternative<std::string>(readRegister(frame, instruction.second).data)) {
        writeRegister(frame, instruction.destination,
                      RuntimeValue(readRegister(frame, instruction.first).display() +
                                   readRegister(frame, instruction.second).display()));
        break;
      }
      [[fallthrough]];
    case OpCode::Subtract:
    case OpCode::Multiply:
    case OpCode::Divide:
    case OpCode::Remainder:
    case OpCode::Less:
    case OpCode::LessEqual:
    case OpCode::Greater:
    case OpCode::GreaterEqual: {
      double left = 0.0;
      double right = 0.0;
      bool leftInteger = false;
      bool rightInteger = false;
      if (!number(readRegister(frame, instruction.first), left, leftInteger) ||
          !number(readRegister(frame, instruction.second), right, rightInteger)) {
        if (auto failure = raise("KVM2002", std::string(opcodeName(instruction.opcode)) +
                                               " requires numeric operands",
                                 instruction.span))
          return *std::move(failure);
        continue;
      }
      if (instruction.opcode == OpCode::Divide && right == 0.0) {
        if (auto failure = raise("KVM2003", "division by zero", instruction.span))
          return *std::move(failure);
        continue;
      }
      if (instruction.opcode == OpCode::Remainder && (!leftInteger || !rightInteger)) {
        if (auto failure = raise("KVM2006", "remainder requires integer operands",
                                 instruction.span))
          return *std::move(failure);
        continue;
      }
      if (instruction.opcode == OpCode::Remainder && right == 0.0) {
        if (auto failure = raise("KVM2007", "remainder by zero", instruction.span))
          return *std::move(failure);
        continue;
      }
      switch (instruction.opcode) {
      case OpCode::Add:
      case OpCode::Subtract:
      case OpCode::Multiply:
        if (leftInteger && rightInteger) {
          const auto integerLeft =
              std::get<std::int64_t>(readRegister(frame, instruction.first).data);
          const auto integerRight =
              std::get<std::int64_t>(readRegister(frame, instruction.second).data);
          std::int64_t integerResult = 0;
          if (!checkedIntegerArithmetic(instruction.opcode, integerLeft, integerRight,
                                        integerResult)) {
            if (auto failure = raise(
                    "KVM2005", "integer overflow while evaluating '" +
                                   std::string(opcodeName(instruction.opcode)) + "'",
                    instruction.span))
              return *std::move(failure);
            continue;
          }
          writeRegister(frame, instruction.destination, RuntimeValue(integerResult));
        } else if (instruction.opcode == OpCode::Add)
          writeRegister(frame, instruction.destination, RuntimeValue(left + right));
        else if (instruction.opcode == OpCode::Subtract)
          writeRegister(frame, instruction.destination, RuntimeValue(left - right));
        else
          writeRegister(frame, instruction.destination, RuntimeValue(left * right));
        break;
      case OpCode::Divide:
        writeRegister(frame, instruction.destination, RuntimeValue(left / right));
        break;
      case OpCode::Remainder: {
        const auto integerLeft =
            std::get<std::int64_t>(readRegister(frame, instruction.first).data);
        const auto integerRight =
            std::get<std::int64_t>(readRegister(frame, instruction.second).data);
        writeRegister(frame, instruction.destination,
                      RuntimeValue(integerLeft == std::numeric_limits<std::int64_t>::min() &&
                                           integerRight == -1
                                       ? std::int64_t{0}
                                       : integerLeft % integerRight));
        break;
      }
      case OpCode::Less:
        writeRegister(frame, instruction.destination,
                      RuntimeValue(leftInteger && rightInteger
                                       ? std::get<std::int64_t>(
                                             readRegister(frame, instruction.first).data) <
                                             std::get<std::int64_t>(
                                                 readRegister(frame, instruction.second).data)
                                       : left < right));
        break;
      case OpCode::LessEqual:
        writeRegister(frame, instruction.destination,
                      RuntimeValue(leftInteger && rightInteger
                                       ? std::get<std::int64_t>(
                                             readRegister(frame, instruction.first).data) <=
                                             std::get<std::int64_t>(
                                                 readRegister(frame, instruction.second).data)
                                       : left <= right));
        break;
      case OpCode::Greater:
        writeRegister(frame, instruction.destination,
                      RuntimeValue(leftInteger && rightInteger
                                       ? std::get<std::int64_t>(
                                             readRegister(frame, instruction.first).data) >
                                             std::get<std::int64_t>(
                                                 readRegister(frame, instruction.second).data)
                                       : left > right));
        break;
      case OpCode::GreaterEqual:
        writeRegister(frame, instruction.destination,
                      RuntimeValue(leftInteger && rightInteger
                                       ? std::get<std::int64_t>(
                                             readRegister(frame, instruction.first).data) >=
                                             std::get<std::int64_t>(
                                                 readRegister(frame, instruction.second).data)
                                       : left >= right));
        break;
      default:
        break;
      }
      break;
    }
    case OpCode::Jump:
      frame.instructionPointer = instruction.first;
      continue;
    case OpCode::JumpIfFalse:
      if (!readRegister(frame, instruction.first).isTruthy()) {
        frame.instructionPointer = instruction.second;
        continue;
      }
      break;
    case OpCode::Call:
    case OpCode::CallIndirect: {
      std::uint32_t targetFunction = instruction.first;
      std::vector<VmCaptureCell *> calledCaptures;
      if (instruction.opcode == OpCode::CallIndirect) {
        const auto &callee = readRegister(frame, instruction.first);
        if (const auto *reference = std::get_if<VmFunctionReference>(&callee.data)) {
          targetFunction = reference->function;
        } else if (const auto *closure = std::get_if<VmClosure *>(&callee.data); closure && *closure) {
          targetFunction = (*closure)->function;
          calledCaptures = (*closure)->captures;
        } else {
          if (auto failure = raise("KVM2010", "value of type '" + callee.typeName() +
                                                          "' is not callable",
                                   instruction.span, callee))
            return *std::move(failure);
          continue;
        }
      }
      const auto &target = module.functions[targetFunction];
      const auto &arguments = module.callArguments[instruction.second];
      if (arguments.size() != target.parameterCount) {
        if (auto failure = raise("KVM2011", "function '" + target.name + "' expects " +
                                                std::to_string(target.parameterCount) +
                                                " argument(s), but " +
                                                std::to_string(arguments.size()) +
                                                " were provided",
                                 instruction.span))
          return *std::move(failure);
        continue;
      }
      if (frames.size() >= MaximumCallFrames) {
        if (auto failure = raise("KVM2004", "maximum call depth of " +
                                                std::to_string(MaximumCallFrames) +
                                                " exceeded while calling '" + target.name + "'",
                                 instruction.span))
          return *std::move(failure);
        continue;
      }
      CallFrame called{targetFunction, std::vector<RuntimeValue>(target.registerCount), 0,
                       instruction.destination, instruction.span, std::move(calledCaptures),
                       std::vector<VmCaptureCell *>(target.registerCount, nullptr)};
      for (std::size_t index = 0; index < arguments.size(); ++index)
        called.registers[index] = readRegister(frame, arguments[index]);
      ++frame.instructionPointer;
      frames.push_back(std::move(called));
      continue;
    }
    case OpCode::CallNative: {
      const auto &name = module.nativeFunctions[instruction.first];
      if (!nativeAdapter) {
        if (auto failure = raise("KVM2021", "native function '" + name +
                                                "' is unavailable in this execution environment",
                                 instruction.span))
          return *std::move(failure);
        continue;
      }
      std::vector<RuntimeValue> arguments;
      arguments.reserve(module.callArguments[instruction.second].size());
      for (const auto argument : module.callArguments[instruction.second])
        arguments.push_back(readRegister(frame, argument));
      auto result = nativeAdapter->invoke(name, arguments, heap);
      if (result.failure) {
        if (auto failure = raise(std::move(result.failure->code),
                                 std::move(result.failure->message), instruction.span,
                                 std::move(result.failure->cause)))
          return *std::move(failure);
        continue;
      }
      writeRegister(frame, instruction.destination, std::move(result.value));
      collectAtSafepoint();
      break;
    }
    case OpCode::LoadMember: {
      const auto &object = readRegister(frame, instruction.first);
      const auto &member = std::get<std::string>(module.constants[instruction.second]);
      if (const auto error = std::get_if<ErrorPtr>(&object.data); error && *error) {
        if (member == "message")
          writeRegister(frame, instruction.destination, RuntimeValue((*error)->message));
        else if (member == "code")
          writeRegister(frame, instruction.destination, RuntimeValue((*error)->code));
        else if (member == "cause")
          writeRegister(frame, instruction.destination, (*error)->cause);
        else {
          if (auto failure = raise("KVM2302", "Error has no member '" + member + "'",
                                   instruction.span, object))
            return *std::move(failure);
          continue;
        }
      } else {
        if (auto failure = raise("KVM2020", "value of type '" + object.typeName() +
                                                "' has no member '" + member + "'",
                                 instruction.span, object))
          return *std::move(failure);
        continue;
      }
      break;
    }
    case OpCode::Throw: {
      const auto &thrown = readRegister(frame, instruction.first);
      ErrorObject *error = nullptr;
      if (const auto existing = std::get_if<ErrorPtr>(&thrown.data); existing && *existing)
        error = *existing;
      else
        error = heap.allocateError(thrown.display(), "KVM2301", thrown);

      if (auto failure = unwindError(error, instruction.span))
        return *std::move(failure);
      continue;
    }
    case OpCode::Return: {
      auto result = readRegister(frame, instruction.first);
      if (frames.size() == 1)
        return {std::move(result), {}, heap.stats()};
      const auto destination = *frame.returnDestination;
      frames.pop_back();
      writeRegister(frames.back(), destination, std::move(result));
      continue;
    }
    }
    ++frame.instructionPointer;
  }
  return {{}, {}, heap.stats()};
}

} // namespace kyna
