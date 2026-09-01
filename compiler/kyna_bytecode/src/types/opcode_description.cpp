#include "kyna/bytecode/bytecode_module.hpp"

namespace kyna {

const char *opcodeName(OpCode opcode) {
  switch (opcode) {
#include "kyna/bytecode/opcode_names.inc"
  }
  return "invalid";
}

} // namespace kyna
