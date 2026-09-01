#include "kyna/mir/mir_program.hpp"

namespace kyna {

const char *mirInstructionName(MirInstructionKind kind) {
  switch (kind) {
  case MirInstructionKind::Constant: return "constant";
  case MirInstructionKind::FunctionReference: return "function";
  case MirInstructionKind::Closure: return "closure";
  case MirInstructionKind::LoadCapture: return "load_capture";
  case MirInstructionKind::StoreCapture: return "store_capture";
  case MirInstructionKind::Move: return "move";
  case MirInstructionKind::Negate: return "negate";
  case MirInstructionKind::Not: return "not";
  case MirInstructionKind::Add: return "add";
  case MirInstructionKind::Subtract: return "subtract";
  case MirInstructionKind::Multiply: return "multiply";
  case MirInstructionKind::Divide: return "divide";
  case MirInstructionKind::Remainder: return "remainder";
  case MirInstructionKind::Equal: return "equal";
  case MirInstructionKind::NotEqual: return "not_equal";
  case MirInstructionKind::Less: return "less";
  case MirInstructionKind::LessEqual: return "less_equal";
  case MirInstructionKind::Greater: return "greater";
  case MirInstructionKind::GreaterEqual: return "greater_equal";
  case MirInstructionKind::Call: return "call";
  case MirInstructionKind::CallIndirect: return "call_indirect";
  case MirInstructionKind::CallNative: return "call_native";
  case MirInstructionKind::LoadMember: return "load_member";
  case MirInstructionKind::BindMethod: return "bind_method";
  case MirInstructionKind::MakeArray: return "make_array";
  case MirInstructionKind::MakeObject: return "make_object";
  case MirInstructionKind::MakeInstance: return "make_instance";
  case MirInstructionKind::LoadIndex: return "load_index";
  case MirInstructionKind::StoreIndex: return "store_index";
  case MirInstructionKind::StoreMember: return "store_member";
  }
  return "unknown";
}

} // namespace kyna
