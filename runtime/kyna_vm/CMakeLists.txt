kyna_add_module(kyna_vm SOURCES
  src/execution/module_program_executor.cpp
  src/execution/statement_executor.cpp
  src/execution/expression_evaluator.cpp
  src/execution/expression_operations.cpp
  src/validators/bytecode_numeric_operations.cpp
  src/rendering/bytecode_runtime_diagnostic.cpp
  src/memory/managed_heap.cpp
  src/values/runtime_value.cpp
  src/values/lexical_environment.cpp
  src/values/function_value.cpp
  src/execution/bytecode_virtual_machine.cpp
  PUBLIC_DEPENDENCIES kyna_typecheck kyna_bytecode kyna_host)
