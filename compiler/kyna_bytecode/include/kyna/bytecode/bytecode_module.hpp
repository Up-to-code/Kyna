#pragma once

#include "kyna/source/source_span.hpp"
#include <cstdint>
#include <string>
#include <optional>
#include <variant>
#include <vector>

namespace kyna {

using BytecodeConstant =
    std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, char>;

enum class OpCode : std::uint8_t {
  LoadConstant,
  LoadNull,
  LoadFunction,
  MakeClosure,
  LoadCapture,
  StoreCapture,
  Move,
  Negate,
  Not,
  Add,
  Subtract,
  Multiply,
  Divide,
  Remainder,
  Equal,
  NotEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  Jump,
  JumpIfFalse,
  Call,
  CallIndirect,
  CallNative,
  LoadMember,
  BindMethod,
  MakeArray,
  MakeObject,
  MakeInstance,
  LoadIndex,
  StoreIndex,
  StoreMember,
  Throw,
  Return,
};

struct BytecodeInstruction {
  OpCode opcode{OpCode::LoadNull};
  std::uint32_t destination{0};
  std::uint32_t first{0};
  std::uint32_t second{0};
  SourceSpan span;
};

struct BytecodeFunction {
  std::string name;
  std::uint32_t registerCount{0};
  std::vector<BytecodeInstruction> instructions;
  std::uint32_t parameterCount{0};
  std::uint32_t captureCount{0};
  struct ExceptionHandler {
    std::uint32_t firstInstruction{0};
    std::uint32_t instructionCount{0};
    std::uint32_t handlerInstruction{0};
    std::uint32_t errorRegister{0};
  };
  std::vector<ExceptionHandler> exceptionHandlers;
};

struct BytecodeCaptureSource {
  enum class Kind : std::uint8_t { Local, Capture };
  Kind kind{Kind::Local};
  std::uint32_t index{0};
};

struct BytecodeClassMethod {
  std::string name;
  std::uint32_t function{0};
};

struct BytecodeClass {
  std::string name;
  std::optional<std::uint32_t> parent;
  std::vector<std::string> fields;
  std::vector<BytecodeClassMethod> methods;
  std::optional<std::uint32_t> constructor;
};

struct BytecodeModule {
  static constexpr std::uint32_t FormatVersion = 7;
  std::uint32_t formatVersion{FormatVersion};
  std::string name;
  std::vector<BytecodeConstant> constants;
  std::vector<std::vector<std::uint32_t>> callArguments;
  std::vector<std::string> nativeFunctions;
  std::vector<std::vector<std::string>> objectFieldNames;
  std::vector<std::vector<BytecodeCaptureSource>> closureCaptures;
  std::vector<BytecodeFunction> functions;
  std::vector<BytecodeClass> classes;
  std::uint32_t entryFunction{0};
};

[[nodiscard]] const char *opcodeName(OpCode opcode);

} // namespace kyna
