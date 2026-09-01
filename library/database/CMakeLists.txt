kyna_add_module(kyna_database_library SOURCES
  src/support/database_scalar.cpp
  src/support/runtime_scalar.cpp
  src/support/database_error.cpp
  src/operations/query.cpp
  PUBLIC_DEPENDENCIES kyna_vm kyna_host)
