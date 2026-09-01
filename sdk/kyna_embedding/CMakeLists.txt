kyna_add_module(kyna_embedding
  SOURCES src/types/result_methods.cpp
          src/session/language_session.cpp
          src/session/check_source.cpp
          src/session/run_source.cpp
          src/session/inspection.cpp
          src/support/has_errors.cpp
          src/support/requires_host_server.cpp
          src/support/escape_json.cpp
          src/support/statement_helpers.cpp
          src/support/standard_library_hir_options.cpp
          src/support/execute_bytecode_subset.cpp
  PUBLIC_DEPENDENCIES kyna_standard_library kyna_resolution kyna_typecheck kyna_hir kyna_mir
                      kyna_bytecode kyna_vm)
