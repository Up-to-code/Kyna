#include "kyna/execution/bytecode_virtual_machine.hpp"
#include "kyna/bytecode/bytecode_validator.hpp"
#include "kyna/memory/tracing_heap.hpp"
#include "../rendering/bytecode_runtime_diagnostic.hpp"
#include "../types/bytecode_call_frame.hpp"
#include "../validators/bytecode_numeric_operations.hpp"
#include <optional>

namespace kyna {
namespace {
} // namespace

BytecodeExecutionResult
BytecodeVirtualMachine::execute(const BytecodeModule &module,
                                BytecodeNativeAdapter *nativeAdapter) const {
  auto validation = validateBytecode(module);
  if (!validation.ok())
    return {{}, std::move(validation.diagnostics)};

  constexpr std::size_t MaximumCallFrames = 4096;
  Heap heap;
  std::vector<BytecodeCallFrame> frames;
  std::size_t exceptionBoundary = 0;
  std::size_t nativeDepth = 0;
  std::function<BytecodeExecutionResult(std::size_t)> executeFrames;
  const auto &entry = module.functions[module.entryFunction];
  frames.push_back(
      {module.entryFunction, std::vector<RuntimeValue>(entry.registerCount), 0, std::nullopt,
       std::nullopt, {}, {}, std::vector<VmCaptureCell *>(entry.registerCount, nullptr)});

  const auto readRegister = [](const BytecodeCallFrame &frame, std::uint32_t index)
      -> const RuntimeValue & {
    return frame.registerCells[index] ? frame.registerCells[index]->value
                                      : frame.registers[index];
  };
  const auto writeRegister = [](BytecodeCallFrame &frame, std::uint32_t index, RuntimeValue value) {
    frame.registers[index] = value;
    if (frame.registerCells[index])
      frame.registerCells[index]->value = std::move(value);
  };
  const auto collectAtSafepoint = [&](bool force = false) {
    HeapRoots roots;
    for (const auto &activeFrame : frames) {
      for (const auto &value : activeFrame.registers)
        roots.values.push_back(&value);
      if (activeFrame.returnOverride)
        roots.values.push_back(&*activeFrame.returnOverride);
      for (auto *cell : activeFrame.registerCells)
        if (cell)
          roots.captureCells.push_back(cell);
      for (auto *cell : activeFrame.captures)
        if (cell)
          roots.captureCells.push_back(cell);
    }
    if (force) heap.collectRoots(roots);
    else heap.maybeCollectRoots(roots);
  };
  const auto findVmMethod = [&](std::uint32_t classIndex, const std::string &name) {
    std::optional<std::uint32_t> cursor{classIndex};
    while (cursor) {
      const auto &klass = module.classes[*cursor];
      for (const auto &method : klass.methods)
        if (method.name == name)
          return std::optional<std::uint32_t>{method.function};
      cursor = klass.parent;
    }
    return std::optional<std::uint32_t>{};
  };
  const auto findVmConstructor = [&](std::uint32_t classIndex) {
    std::optional<std::uint32_t> cursor{classIndex};
    while (cursor) {
      const auto &klass = module.classes[*cursor];
      if (klass.constructor) return klass.constructor;
      cursor = klass.parent;
    }
    return std::optional<std::uint32_t>{};
  };
  const auto initializeVmFields = [&](auto &&self, std::uint32_t classIndex,
                                      Object *instance) -> void {
    const auto &klass = module.classes[classIndex];
    if (klass.parent)
      self(self, *klass.parent, instance);
    for (const auto &field : klass.fields)
      instance->fields.try_emplace(field, RuntimeValue());
  };
  const auto unwindError = [&](ErrorObject *error, SourceSpan span)
      -> std::optional<BytecodeExecutionResult> {
    std::optional<std::size_t> handledFrame;
    const BytecodeFunction::ExceptionHandler *matched = nullptr;
    std::size_t faultInstruction = frames.back().instructionPointer;
    for (std::size_t count = frames.size(); count > exceptionBoundary; --count) {
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
      auto diagnostic = makeBytecodeRuntimeDiagnostic(error->code.empty() ? "KVM2301" : error->code,
                                                      error->message, span, module, frames);
      if (!std::holds_alternative<std::nullptr_t>(error->cause.data))
        diagnostic.notes.push_back("cause: " + error->cause.display());
      diagnostic.help = "catch this error with 'try/catch' or fix the failing operation";
      return BytecodeExecutionResult{RuntimeValue(error), {std::move(diagnostic)}, heap.stats()};
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

  executeFrames = [&](std::size_t baseDepth) -> BytecodeExecutionResult {
  while (frames.size() > baseDepth) {
    auto &frame = frames.back();
    const auto &function = module.functions[frame.function];
    if (frame.instructionPointer >= function.instructions.size())
      return {{}, {makeBytecodeRuntimeDiagnostic("KVM2099", "instruction pointer escaped function '" +
                                                   function.name + "'",
                                                 function.instructions.back().span, module, frames)}};

    const auto &instruction = function.instructions[frame.instructionPointer];
    switch (instruction.opcode) {
    case OpCode::LoadConstant:
      writeRegister(frame, instruction.destination,
                    bytecodeConstantValue(module.constants[instruction.first]));
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
          if (auto failure = raise("KRT2204", "integer overflow while negating value",
                                   instruction.span))
            return *std::move(failure);
          continue;
        }
        writeRegister(frame, instruction.destination, RuntimeValue(-*integer));
      } else if (const auto floating =
                     std::get_if<double>(&readRegister(frame, instruction.first).data))
        writeRegister(frame, instruction.destination, RuntimeValue(-*floating));
      else {
        if (auto failure = raise("KRT2200", "negate requires a numeric operand",
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
      if (!bytecodeNumber(readRegister(frame, instruction.first), left, leftInteger) ||
          !bytecodeNumber(readRegister(frame, instruction.second), right, rightInteger)) {
        if (auto failure = raise("KRT2200", std::string(opcodeName(instruction.opcode)) +
                                               " requires numeric operands",
                                 instruction.span))
          return *std::move(failure);
        continue;
      }
      if (instruction.opcode == OpCode::Divide && right == 0.0) {
        if (auto failure = raise("KRT2201", "division by zero", instruction.span))
          return *std::move(failure);
        continue;
      }
      if (instruction.opcode == OpCode::Remainder && (!leftInteger || !rightInteger)) {
        if (auto failure = raise("KRT2202", "remainder requires integer operands",
                                 instruction.span))
          return *std::move(failure);
        continue;
      }
      if (instruction.opcode == OpCode::Remainder && right == 0.0) {
        if (auto failure = raise("KRT2201", "remainder by zero", instruction.span))
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
          if (!checkedBytecodeIntegerArithmetic(instruction.opcode, integerLeft, integerRight,
                                                integerResult)) {
            if (auto failure = raise(
                    "KRT2204", "integer overflow while evaluating '" +
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
      ObjectPtr boundReceiver = nullptr;
      if (instruction.opcode == OpCode::CallIndirect) {
        const auto &callee = readRegister(frame, instruction.first);
        if (const auto *reference = std::get_if<VmFunctionReference>(&callee.data)) {
          targetFunction = reference->function;
        } else if (const auto *closure = std::get_if<VmClosure *>(&callee.data); closure && *closure) {
          targetFunction = (*closure)->function;
          calledCaptures = (*closure)->captures;
        } else if (const auto *method = std::get_if<VmBoundMethod *>(&callee.data);
                   method && *method) {
          targetFunction = (*method)->function;
          boundReceiver = (*method)->receiver;
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
      const auto suppliedArguments = arguments.size() + (boundReceiver ? 1u : 0u);
      if (suppliedArguments != target.parameterCount) {
        if (auto failure = raise("KVM2011", "function '" + target.name + "' expects " +
                                                std::to_string(target.parameterCount) +
                                                " argument(s), but " +
                                                std::to_string(suppliedArguments) +
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
      BytecodeCallFrame called{targetFunction, std::vector<RuntimeValue>(target.registerCount), 0,
                       instruction.destination, std::nullopt, instruction.span,
                       std::move(calledCaptures),
                       std::vector<VmCaptureCell *>(target.registerCount, nullptr)};
      std::size_t firstArgument = 0;
      if (boundReceiver) {
        called.registers[0] = RuntimeValue(boundReceiver);
        firstArgument = 1;
      }
      for (std::size_t index = 0; index < arguments.size(); ++index)
        called.registers[index + firstArgument] = readRegister(frame, arguments[index]);
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
      const auto destination = instruction.destination;
      const auto span = instruction.span;
      auto roots = heap.rootScope();
      for (const auto &argument : arguments) roots.protect(argument);
      const auto targetOf = [&](const RuntimeValue &callee) -> std::optional<std::uint32_t> {
        if (const auto ref = std::get_if<VmFunctionReference>(&callee.data)) return ref->function;
        if (const auto closure = std::get_if<VmClosure *>(&callee.data); closure && *closure)
          return (*closure)->function;
        if (const auto method = std::get_if<VmBoundMethod *>(&callee.data); method && *method)
          return (*method)->function;
        return std::nullopt;
      };
      NativeCallbacks callbacks;
      callbacks.collect = [&] { collectAtSafepoint(true); };
      callbacks.arity = [&](const RuntimeValue &callee) -> std::optional<std::size_t> {
        const auto target = targetOf(callee);
        if (!target || *target >= module.functions.size()) return std::nullopt;
        const bool bound = std::holds_alternative<VmBoundMethod *>(callee.data);
        const auto count = module.functions[*target].parameterCount;
        if (bound && count == 0) return std::nullopt;
        return count - (bound ? 1u : 0u);
      };
      callbacks.invoke = [&](const RuntimeValue &callee, std::span<const RuntimeValue> values)
          -> NativeCallResult {
        const auto arity = callbacks.arity(callee);
        if (!arity) return {{}, NativeCallFailure{"KVM2010", "value is not callable", callee}};
        if (*arity != values.size())
          return {{}, NativeCallFailure{"KVM2011", "callback argument count mismatch", {}}};
        if (frames.size() >= MaximumCallFrames || nativeDepth >= 64)
          return {{}, NativeCallFailure{"KVM2004", "maximum native callback depth exceeded", {}}};
        const auto targetIndex = *targetOf(callee);
        const auto &target = module.functions[targetIndex];
        BytecodeCallFrame called{targetIndex, std::vector<RuntimeValue>(target.registerCount),
            0, std::nullopt, std::nullopt, span, {},
            std::vector<VmCaptureCell *>(target.registerCount, nullptr)};
        std::size_t offset = 0;
        if (const auto closure = std::get_if<VmClosure *>(&callee.data))
          called.captures = (*closure)->captures;
        if (const auto method = std::get_if<VmBoundMethod *>(&callee.data)) {
          called.registers[0] = RuntimeValue((*method)->receiver);
          offset = 1;
        }
        for (std::size_t i = 0; i < values.size(); ++i) called.registers[i + offset] = values[i];
        const auto boundary = frames.size();
        const auto savedBoundary = exceptionBoundary;
        exceptionBoundary = boundary;
        ++nativeDepth;
        frames.push_back(std::move(called));
        auto outcome = executeFrames(boundary);
        frames.resize(boundary);
        --nativeDepth;
        exceptionBoundary = savedBoundary;
        if (!outcome.ok()) {
          if (const auto error = std::get_if<ErrorObject *>(&outcome.value.data); error && *error)
            return {{}, NativeCallFailure{(*error)->code, (*error)->message, (*error)->cause,
                                          std::move(outcome.diagnostics.front())}};
          const auto &diagnostic = outcome.diagnostics.front();
          return {{}, NativeCallFailure{diagnostic.code, diagnostic.message, {}, diagnostic}};
        }
        return {std::move(outcome.value), std::nullopt};
      };
      auto result = nativeAdapter->invokeWithCallbacks(name, arguments, heap, callbacks);
      if (result.failure) {
        auto callbackDiagnostic = std::move(result.failure->diagnostic);
        if (auto failure = raise(std::move(result.failure->code),
                                 std::move(result.failure->message), span,
                                 std::move(result.failure->cause))) {
          if (callbackDiagnostic) {
            failure->diagnostics.front().location = callbackDiagnostic->location;
            failure->diagnostics.front().callFrames = std::move(callbackDiagnostic->callFrames);
          }
          return *std::move(failure);
        }
        continue;
      }
      writeRegister(frames.back(), destination, std::move(result.value));
      collectAtSafepoint();
      ++frames.back().instructionPointer;
      continue;
    }
    case OpCode::LoadMember: {
      const auto &object = readRegister(frame, instruction.first);
      const auto &member = std::get<std::string>(module.constants[instruction.second]);
      if (const auto instance = std::get_if<ObjectPtr>(&object.data); instance && *instance) {
        const auto found = (*instance)->fields.find(member);
        if (found != (*instance)->fields.end())
          writeRegister(frame, instruction.destination, found->second);
        else if ((*instance)->vmClass) {
          const auto method = findVmMethod(*(*instance)->vmClass, member);
          if (method)
            writeRegister(frame, instruction.destination,
                          RuntimeValue(heap.allocateBoundMethod(*instance, *method)));
          else {
            if (auto failure = raise("KRT2002", "object has no member '" + member + "'",
                                     instruction.span, object))
              return *std::move(failure);
            continue;
          }
        } else {
          if (auto failure = raise("KRT2002", "object has no member '" + member + "'",
                                   instruction.span, object))
            return *std::move(failure);
          continue;
        }
      } else if (const auto error = std::get_if<ErrorPtr>(&object.data); error && *error) {
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
    case OpCode::BindMethod: {
      const auto &receiver = readRegister(frame, instruction.first);
      const auto instance = std::get_if<ObjectPtr>(&receiver.data);
      if (!instance || !*instance) {
        if (auto failure = raise("KVM2203", "method receiver must be an object, got '" +
                                                receiver.typeName() + "'",
                                 instruction.span, receiver))
          return *std::move(failure);
        continue;
      }
      writeRegister(frame, instruction.destination,
                    RuntimeValue(heap.allocateBoundMethod(*instance, instruction.second)));
      break;
    }
    case OpCode::MakeInstance: {
      const auto &klass = module.classes[instruction.first];
      auto *instance = heap.allocate();
      instance->vmClass = instruction.first;
      instance->vmClassName = klass.name;
      initializeVmFields(initializeVmFields, instruction.first, instance);
      const auto &arguments = module.callArguments[instruction.second];
      const auto constructorIndex = findVmConstructor(instruction.first);
      if (!constructorIndex) {
        if (!arguments.empty()) {
          if (auto failure = raise("KVM2201", "class '" + klass.name +
                                                  "' has no constructor but received " +
                                                  std::to_string(arguments.size()) +
                                                  " argument(s)",
                                   instruction.span, RuntimeValue(instance)))
            return *std::move(failure);
          continue;
        }
        writeRegister(frame, instruction.destination, RuntimeValue(instance));
        collectAtSafepoint();
        break;
      }
      const auto &constructor = module.functions[*constructorIndex];
      if (constructor.parameterCount != arguments.size() + 1) {
        if (auto failure = raise("KVM2202", "constructor for '" + klass.name + "' expects " +
                                                std::to_string(constructor.parameterCount - 1) +
                                                " argument(s), but " +
                                                std::to_string(arguments.size()) +
                                                " were provided",
                                 instruction.span, RuntimeValue(instance)))
          return *std::move(failure);
        continue;
      }
      if (frames.size() >= MaximumCallFrames) {
        if (auto failure = raise("KVM2004", "maximum call depth of " +
                                                std::to_string(MaximumCallFrames) +
                                                " exceeded while constructing '" + klass.name +
                                                "'",
                                 instruction.span, RuntimeValue(instance)))
          return *std::move(failure);
        continue;
      }
      BytecodeCallFrame called{*constructorIndex,
                       std::vector<RuntimeValue>(constructor.registerCount), 0,
                       instruction.destination, RuntimeValue(instance), instruction.span, {},
                       std::vector<VmCaptureCell *>(constructor.registerCount, nullptr)};
      called.registers[0] = RuntimeValue(instance);
      for (std::size_t index = 0; index < arguments.size(); ++index)
        called.registers[index + 1] = readRegister(frame, arguments[index]);
      ++frame.instructionPointer;
      frames.push_back(std::move(called));
      continue;
    }
    case OpCode::MakeArray: {
      auto *array = heap.allocateArray();
      array->elements.reserve(module.callArguments[instruction.first].size());
      for (const auto element : module.callArguments[instruction.first])
        array->elements.push_back(readRegister(frame, element));
      writeRegister(frame, instruction.destination, RuntimeValue(array));
      collectAtSafepoint();
      break;
    }
    case OpCode::MakeObject: {
      auto *object = heap.allocate();
      const auto &values = module.callArguments[instruction.first];
      const auto &names = module.objectFieldNames[instruction.second];
      for (std::size_t index = 0; index < names.size(); ++index)
        object->fields.insert_or_assign(names[index], readRegister(frame, values[index]));
      writeRegister(frame, instruction.destination, RuntimeValue(object));
      collectAtSafepoint();
      break;
    }
    case OpCode::LoadIndex: {
      const auto &object = readRegister(frame, instruction.first);
      const auto &index = readRegister(frame, instruction.second);
      if (std::holds_alternative<std::nullptr_t>(object.data)) {
        if (auto failure = raise("KRT2101", "cannot index null", instruction.span))
          return *std::move(failure);
        continue;
      }
      if (const auto array = std::get_if<ArrayPtr>(&object.data); array && *array) {
        const auto position = std::get_if<std::int64_t>(&index.data);
        if (!position) {
          if (auto failure = raise("KRT2103", "array index must be an integer, got '" +
                                                  index.typeName() + "'",
                                   instruction.span, index))
            return *std::move(failure);
          continue;
        }
        if (*position < 0 || static_cast<std::size_t>(*position) >= (*array)->elements.size()) {
          if (auto failure = raise("KRT2104", "array index " + std::to_string(*position) +
                                                  " is out of bounds for length " +
                                                  std::to_string((*array)->elements.size()),
                                   instruction.span, index))
            return *std::move(failure);
          continue;
        }
        writeRegister(frame, instruction.destination,
                      (*array)->elements[static_cast<std::size_t>(*position)]);
        break;
      }
      if (const auto instance = std::get_if<ObjectPtr>(&object.data); instance && *instance) {
        const auto key = std::get_if<std::string>(&index.data);
        if (!key) {
          if (auto failure = raise("KRT2103", "object key must be a string, got '" +
                                                  index.typeName() + "'",
                                   instruction.span, index))
            return *std::move(failure);
          continue;
        }
        const auto found = (*instance)->fields.find(*key);
        if (found == (*instance)->fields.end()) {
          if (auto failure = raise("KRT2105", "object has no field '" + *key + "'",
                                   instruction.span, index))
            return *std::move(failure);
          continue;
        }
        writeRegister(frame, instruction.destination, found->second);
        break;
      }
      if (auto failure = raise("KRT2102", "cannot index value of type '" + object.typeName() +
                                              "'",
                               instruction.span, object))
        return *std::move(failure);
      continue;
    }
    case OpCode::StoreIndex: {
      const auto &operands = module.callArguments[instruction.first];
      const auto &object = readRegister(frame, operands[0]);
      const auto &index = readRegister(frame, operands[1]);
      const auto value = readRegister(frame, operands[2]);
      if (const auto array = std::get_if<ArrayPtr>(&object.data); array && *array) {
        const auto position = std::get_if<std::int64_t>(&index.data);
        if (!position) {
          if (auto failure = raise("KRT2103", "array index must be an integer, got '" +
                                                  index.typeName() + "'",
                                   instruction.span, index))
            return *std::move(failure);
          continue;
        }
        if (*position < 0 || static_cast<std::size_t>(*position) >= (*array)->elements.size()) {
          if (auto failure = raise("KRT2104", "array index " + std::to_string(*position) +
                                                  " is out of bounds for length " +
                                                  std::to_string((*array)->elements.size()),
                                   instruction.span, index))
            return *std::move(failure);
          continue;
        }
        (*array)->elements[static_cast<std::size_t>(*position)] = value;
        writeRegister(frame, instruction.destination, value);
        break;
      }
      if (const auto instance = std::get_if<ObjectPtr>(&object.data); instance && *instance) {
        const auto key = std::get_if<std::string>(&index.data);
        if (!key) {
          if (auto failure = raise("KRT2103", "object key must be a string, got '" +
                                                  index.typeName() + "'",
                                   instruction.span, index))
            return *std::move(failure);
          continue;
        }
        const auto found = (*instance)->fields.find(*key);
        if (found == (*instance)->fields.end()) {
          if (auto failure = raise("KRT2105", "unknown field '" + *key +
                                                  "' on closed object",
                                   instruction.span, index))
            return *std::move(failure);
          continue;
        }
        found->second = value;
        writeRegister(frame, instruction.destination, value);
        break;
      }
      if (auto failure = raise("KRT2102", "cannot assign through value of type '" +
                                              object.typeName() + "'",
                               instruction.span, object))
        return *std::move(failure);
      continue;
    }
    case OpCode::StoreMember: {
      const auto &operands = module.callArguments[instruction.first];
      const auto &object = readRegister(frame, operands[0]);
      const auto value = readRegister(frame, operands[1]);
      const auto &member = std::get<std::string>(module.constants[instruction.second]);
      const auto instance = std::get_if<ObjectPtr>(&object.data);
      if (!instance || !*instance) {
        if (auto failure = raise("KRT2003", "member assignment requires an object, got '" +
                                                object.typeName() + "'",
                                 instruction.span, object))
          return *std::move(failure);
        continue;
      }
      const auto found = (*instance)->fields.find(member);
      if (found == (*instance)->fields.end()) {
        if (auto failure = raise("KRT2004", "unknown field '" + member +
                                                "' on closed object",
                                 instruction.span, object))
          return *std::move(failure);
        continue;
      }
      found->second = value;
      writeRegister(frame, instruction.destination, value);
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
      auto result = frame.returnOverride ? *frame.returnOverride
                                         : readRegister(frame, instruction.first);
      if (frames.size() == baseDepth + 1)
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
  };
  return executeFrames(0);
}

} // namespace kyna
