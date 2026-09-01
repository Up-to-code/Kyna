#include "bytecode_numeric_operations.hpp"
#include <limits>

namespace kyna {

RuntimeValue bytecodeConstantValue(const BytecodeConstant &constant) {
  return std::visit([](const auto &value) { return RuntimeValue(value); }, constant);
}

bool bytecodeNumber(const RuntimeValue &value, double &result, bool &integer) {
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

bool checkedBytecodeIntegerArithmetic(OpCode opcode, std::int64_t left,
                                      std::int64_t right, std::int64_t &result) {
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

} // namespace kyna
