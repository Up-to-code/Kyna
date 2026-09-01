kyna_add_module(kyna_hir SOURCES
  src/lowering/syntax_lowering.cpp
  src/lowering/expression_lowering.cpp
  src/lowering/statement_lowering.cpp
  src/rendering/hir_renderer.cpp
  PUBLIC_DEPENDENCIES kyna_diagnostics kyna_syntax)
