kyna_add_module(kyna_bytecode SOURCES
  src/validators/bytecode_validator.cpp
  src/rendering/bytecode_disassembler.cpp
  src/generators/program_bytecode_compiler.cpp
  src/generators/body_compiler.cpp
  src/types/opcode_description.cpp
  PUBLIC_DEPENDENCIES kyna_diagnostics kyna_mir)

set(KYNA_GENERATED_OPCODE_NAMES
    "${CMAKE_BINARY_DIR}/generated/kyna/bytecode/opcode_names.inc")
add_custom_command(
  OUTPUT "${KYNA_GENERATED_OPCODE_NAMES}"
  COMMAND "${Python3_EXECUTABLE}"
          "${PROJECT_SOURCE_DIR}/build_tools/generate_opcode_names.py"
          "${PROJECT_SOURCE_DIR}/spec/bytecode/opcodes.json"
          "${KYNA_GENERATED_OPCODE_NAMES}"
  DEPENDS
    "${PROJECT_SOURCE_DIR}/build_tools/generate_opcode_names.py"
    "${PROJECT_SOURCE_DIR}/spec/bytecode/opcodes.json"
  VERBATIM)
add_custom_target(kyna_generate_opcode_names DEPENDS "${KYNA_GENERATED_OPCODE_NAMES}")
add_dependencies(kyna_bytecode kyna_generate_opcode_names)
target_include_directories(kyna_bytecode PRIVATE "${CMAKE_BINARY_DIR}/generated")
