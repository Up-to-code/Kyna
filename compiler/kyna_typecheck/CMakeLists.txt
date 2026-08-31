kyna_add_module(kyna_typecheck SOURCES
  src/analyzer.cpp
  src/type_checker.cpp
  src/best_practice_checker.cpp
  src/modifier_rules.cpp
  src/interface_catalog.cpp
  src/program_validation.cpp
  src/module_analyzer.cpp
  PUBLIC_DEPENDENCIES kyna_resolution kyna_symbols kyna_syntax kyna_types kyna_diagnostics)
