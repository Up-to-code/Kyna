#pragma once

#include "kyna/bytecode/bytecode_module.hpp"
#include "kyna/mir/mir_program.hpp"

namespace kyna::bytecode_generation_detail {

void compileBody(BytecodeModule &module, BytecodeFunction &function,
                 const std::vector<MirBasicBlock> &blocks,
                 const std::vector<MirExceptionRegion> &exceptionRegions);

} // namespace kyna::bytecode_generation_detail
