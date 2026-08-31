kyna_add_module(kyna_standard_library SOURCES
  src/standard_library_catalog.cpp
  src/bytecode_standard_library.cpp
  src/json_value_codec.cpp
  PUBLIC_DEPENDENCIES kyna_vm kyna_host kyna_collections_library kyna_database_library)
