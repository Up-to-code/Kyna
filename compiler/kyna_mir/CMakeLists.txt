kyna_add_module(kyna_mir SOURCES
  src/lowering/hir_lowering.cpp
  src/lowering/expression_lowering.cpp
  src/lowering/expression_invocation_lowering.cpp
  src/lowering/expression_control_lowering.cpp
  src/lowering/statement_lowering.cpp
  src/rendering/mir_renderer.cpp
  src/types/mir_instruction_description.cpp
  src/validators/mir_verifier.cpp
  PUBLIC_DEPENDENCIES kyna_diagnostics kyna_hir)
