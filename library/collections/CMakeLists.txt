kyna_add_module(kyna_collections_library SOURCES
  src/catalog/collections_library.cpp
  src/support/argument_validators.cpp
  src/operations/map.cpp
  src/operations/reduce.cpp
  src/operations/find.cpp
  src/operations/any.cpp
  src/operations/all.cpp
  src/operations/unique.cpp
  PUBLIC_DEPENDENCIES kyna_vm)
