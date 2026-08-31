#include "kyna/mir/mir_renderer.hpp"
#include <sstream>
#include <type_traits>

namespace kyna {
namespace {
std::string constantText(const HirConstant &constant) {
  return std::visit(
      [](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>)
          return "null";
        else if constexpr (std::is_same_v<T, bool>)
          return value ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::string>)
          return '"' + value + '"';
        else if constexpr (std::is_same_v<T, char>)
          return std::string("'") + value + "'";
        else
          return std::to_string(value);
      },
      constant);
}

void renderBody(std::ostringstream &output, const std::vector<MirBasicBlock> &blocks,
                const std::vector<MirExceptionRegion> &exceptionRegions) {
  for (std::size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    const auto &block = blocks[blockIndex];
    output << "bb" << blockIndex << ":\n";
    for (const auto &instruction : block.instructions) {
      output << "  %t" << instruction.destination.value << " = "
             << mirInstructionName(instruction.kind);
      if (instruction.kind == MirInstructionKind::Constant)
        output << ' ' << constantText(instruction.constant);
      else if (instruction.kind == MirInstructionKind::FunctionReference)
        output << " @f" << instruction.function;
      else if (instruction.kind == MirInstructionKind::Closure) {
        output << " @f" << instruction.function << " captures(";
        for (std::size_t capture = 0; capture < instruction.captureSources.size(); ++capture) {
          if (capture) output << ", ";
          const auto &source = instruction.captureSources[capture];
          output << (source.kind == MirCaptureSource::Kind::Local ? "local %t" : "capture ")
                 << source.index;
        }
        output << ')';
      } else if (instruction.kind == MirInstructionKind::LoadCapture)
        output << " capture " << instruction.capture;
      else if (instruction.kind == MirInstructionKind::StoreCapture)
        output << " capture " << instruction.capture << ", %t" << instruction.first.value;
      else if (instruction.kind == MirInstructionKind::Call) {
        output << " @f" << instruction.function << '(';
        for (std::size_t argument = 0; argument < instruction.arguments.size(); ++argument) {
          if (argument) output << ", ";
          output << "%t" << instruction.arguments[argument].value;
        }
        output << ')';
      } else if (instruction.kind == MirInstructionKind::CallIndirect) {
        output << " %t" << instruction.first.value << '(';
        for (std::size_t argument = 0; argument < instruction.arguments.size(); ++argument) {
          if (argument) output << ", ";
          output << "%t" << instruction.arguments[argument].value;
        }
        output << ')';
      } else if (instruction.kind == MirInstructionKind::CallNative) {
        output << ' ' << std::get<std::string>(instruction.constant) << '(';
        for (std::size_t argument = 0; argument < instruction.arguments.size(); ++argument) {
          if (argument) output << ", ";
          output << "%t" << instruction.arguments[argument].value;
        }
        output << ')';
      } else if (instruction.kind == MirInstructionKind::LoadMember) {
        output << " %t" << instruction.first.value << ", "
               << std::get<std::string>(instruction.constant);
      } else if (instruction.kind == MirInstructionKind::MakeArray ||
                 instruction.kind == MirInstructionKind::MakeObject) {
        output << (instruction.kind == MirInstructionKind::MakeArray ? " [" : " {");
        for (std::size_t index = 0; index < instruction.arguments.size(); ++index) {
          if (index) output << ", ";
          if (instruction.kind == MirInstructionKind::MakeObject)
            output << instruction.names[index] << ": ";
          output << "%t" << instruction.arguments[index].value;
        }
        output << (instruction.kind == MirInstructionKind::MakeArray ? ']' : '}');
      } else if (instruction.kind == MirInstructionKind::MakeInstance) {
        output << " @c" << instruction.function << '(';
        for (std::size_t argument = 0; argument < instruction.arguments.size(); ++argument) {
          if (argument) output << ", ";
          output << "%t" << instruction.arguments[argument].value;
        }
        output << ')';
      } else if (instruction.kind == MirInstructionKind::LoadIndex) {
        output << " %t" << instruction.first.value << ", %t" << instruction.second.value;
      } else if (instruction.kind == MirInstructionKind::StoreIndex ||
                 instruction.kind == MirInstructionKind::StoreMember) {
        output << ' ';
        for (std::size_t index = 0; index < instruction.arguments.size(); ++index) {
          if (index) output << ", ";
          output << "%t" << instruction.arguments[index].value;
        }
        if (instruction.kind == MirInstructionKind::StoreMember)
          output << ", " << std::get<std::string>(instruction.constant);
      } else {
        output << " %t" << instruction.first.value;
        if (instruction.kind != MirInstructionKind::Move &&
            instruction.kind != MirInstructionKind::Negate &&
            instruction.kind != MirInstructionKind::Not)
          output << ", %t" << instruction.second.value;
      }
      output << '\n';
    }
    if (!block.terminator)
      continue;
    output << "  ";
    std::visit(
        [&](const auto &terminator) {
          using T = std::decay_t<decltype(terminator)>;
          if constexpr (std::is_same_v<T, MirReturnTerminator>)
            output << "return %t" << terminator.value.value;
          else if constexpr (std::is_same_v<T, MirGotoTerminator>)
            output << "goto bb" << terminator.target.value;
          else if constexpr (std::is_same_v<T, MirBranchTerminator>)
            output << "branch %t" << terminator.condition.value << ", bb"
                   << terminator.trueBlock.value << ", bb" << terminator.falseBlock.value;
          else
            output << "throw %t" << terminator.value.value;
        },
        block.terminator->node);
    output << '\n';
  }
  for (const auto &region : exceptionRegions) {
    output << "  exception [";
    for (std::size_t index = 0; index < region.protectedBlocks.size(); ++index) {
      if (index) output << ", ";
      output << "bb" << region.protectedBlocks[index].value;
    }
    output << "] -> bb" << region.handler.value << " error=%t"
           << region.errorDestination.value << '\n';
  }
}
} // namespace

std::string renderMir(const MirProgram &program) {
  std::ostringstream output;
  output << "mir.module " << program.name << " temporaries=" << program.temporaryCount << '\n';
  renderBody(output, program.blocks, program.exceptionRegions);
  for (std::size_t index = 0; index < program.functions.size(); ++index) {
    const auto &function = program.functions[index];
    output << "\nmir.function @f" << index + 1 << ' ' << function.name
           << " parameters=" << function.parameterCount
           << " captures=" << function.captures.size()
           << " temporaries=" << function.temporaryCount << '\n';
    renderBody(output, function.blocks, function.exceptionRegions);
  }
  for (std::size_t index = 0; index < program.classes.size(); ++index) {
    const auto &klass = program.classes[index];
    output << "\nmir.class @c" << index << ' ' << klass.name;
    if (klass.parent) output << " extends @c" << *klass.parent;
    if (klass.constructor) output << " constructor=@f" << *klass.constructor + 1;
    output << '\n';
    for (const auto &field : klass.fields) output << "  field " << field << '\n';
    for (const auto &method : klass.methods)
      output << "  method " << method.name << " @f" << method.function + 1 << '\n';
  }
  return output.str();
}

} // namespace kyna
