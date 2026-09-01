#pragma once

#include "kyna/bytecode/bytecode_module.hpp"
#include "kyna/execution/runtime_value.hpp"
#include <cstdint>

namespace kyna {

RuntimeValue bytecodeConstantValue(const BytecodeConstant &constant);
bool bytecodeNumber(const RuntimeValue &value, double &result, bool &integer);
bool checkedBytecodeIntegerArithmetic(OpCode opcode, std::int64_t left,
                                      std::int64_t right, std::int64_t &result);

} // namespace kyna
