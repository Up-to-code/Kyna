kyna_add_module(kyna_typecheck SOURCES
  src/checkers/analyzer.cpp
  src/checkers/statement_checker.cpp
  src/checkers/interface_checker.cpp
  src/checkers/type_checker.cpp
  src/checkers/best_practice_checker.cpp
  src/validators/modifier_rules.cpp
  src/catalog/interface_catalog.cpp
  src/validators/program_validation.cpp
  src/checkers/module_analyzer.cpp
  PUBLIC_DEPENDENCIES kyna_resolution kyna_symbols kyna_syntax kyna_types kyna_diagnostics)
