#pragma once

#include "kyna/hir/hir_program.hpp"
#include "kyna/source/source_span.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <utility>

namespace kyna {

struct MirTemporary {
  std::uint32_t value{0};
  auto operator<=>(const MirTemporary &) const = default;
};

struct MirBlockId {
  std::uint32_t value{0};
  auto operator<=>(const MirBlockId &) const = default;
};

enum class MirInstructionKind {
  Constant,
  FunctionReference,
  Closure,
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
  Call,
  CallIndirect,
  CallNative,
  LoadMember,
  MakeArray,
  MakeObject,
  LoadIndex,
  StoreIndex,
  StoreMember
};

struct MirCaptureSource {
  enum class Kind { Local, Capture };
  Kind kind{Kind::Local};
  std::uint32_t index{0};
};

struct MirInstruction {
  MirInstructionKind kind{MirInstructionKind::Constant};
  MirTemporary destination;
  MirTemporary first;
  MirTemporary second;
  HirConstant constant{nullptr};
  SourceSpan span;
  std::uint32_t function{0};
  std::vector<MirTemporary> arguments;
  std::uint32_t capture{0};
  std::vector<MirCaptureSource> captureSources;
  std::vector<std::string> names;

  MirInstruction() = default;
  MirInstruction(MirInstructionKind instructionKind, MirTemporary result,
                 MirTemporary left = {}, MirTemporary right = {},
                 HirConstant value = nullptr, SourceSpan source = {},
                 std::uint32_t targetFunction = 0,
                 std::vector<MirTemporary> callArguments = {},
                 std::uint32_t captureIndex = 0,
                 std::vector<MirCaptureSource> sources = {},
                 std::vector<std::string> fieldNames = {})
      : kind(instructionKind), destination(result), first(left), second(right),
        constant(std::move(value)), span(source), function(targetFunction),
        arguments(std::move(callArguments)), capture(captureIndex),
        captureSources(std::move(sources)), names(std::move(fieldNames)) {}
};

struct MirReturnTerminator {
  MirTemporary value;
};
struct MirGotoTerminator {
  MirBlockId target;
};
struct MirBranchTerminator {
  MirTemporary condition;
  MirBlockId trueBlock;
  MirBlockId falseBlock;
};
struct MirThrowTerminator {
  MirTemporary value;
};

struct MirTerminator {
  using Node =
      std::variant<MirReturnTerminator, MirGotoTerminator, MirBranchTerminator,
                   MirThrowTerminator>;
  Node node;
  SourceSpan span;
};

struct MirExceptionRegion {
  std::vector<MirBlockId> protectedBlocks;
  MirBlockId handler;
  MirTemporary errorDestination;
};

struct MirBasicBlock {
  std::vector<MirInstruction> instructions;
  std::optional<MirTerminator> terminator;
};

struct MirFunction {
  std::string name;
  std::uint32_t parameterCount{0};
  std::uint32_t temporaryCount{0};
  MirBlockId entryBlock;
  std::vector<MirBasicBlock> blocks;
  std::vector<MirExceptionRegion> exceptionRegions;
  SourceSpan span;
  std::vector<HirLocalId> captures;
};

struct MirProgram {
  std::string name;
  std::uint32_t temporaryCount{0};
  MirBlockId entryBlock;
  std::vector<MirBasicBlock> blocks;
  std::vector<MirExceptionRegion> exceptionRegions;
  std::vector<MirFunction> functions;
};

[[nodiscard]] const char *mirInstructionName(MirInstructionKind kind);

} // namespace kyna
