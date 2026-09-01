kyna_add_module(kyna_standard_library SOURCES
  src/catalog/standard_library_catalog.cpp
  src/catalog/console_library.cpp
  src/catalog/text_library.cpp
  src/catalog/collections_library.cpp
  src/catalog/filesystem_library.cpp
  src/catalog/process_host_library.cpp
  src/catalog/formats_library.cpp
  src/catalog/api_store_library.cpp
  src/catalog/network/network_library.cpp
  src/catalog/network/http_server.cpp
  src/bytecode/bytecode_standard_library.cpp
  src/bytecode/failure_support.cpp
  src/bytecode/console_invoke.cpp
  src/bytecode/text_invoke.cpp
  src/bytecode/filesystem_invoke.cpp
  src/bytecode/process_host_invoke.cpp
  src/bytecode/network_invoke.cpp
  src/bytecode/formats_invoke.cpp
  src/bytecode/collections_invoke.cpp
  src/codecs/format/format_value_codec.cpp
  src/codecs/json/json_value_codec.cpp
  PUBLIC_DEPENDENCIES kyna_vm kyna_host kyna_collections_library kyna_database_library
                      kyna_formats_library
                      kyna_text_library)
