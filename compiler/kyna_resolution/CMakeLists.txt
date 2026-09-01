kyna_add_module(kyna_resolution SOURCES
  src/loading/filesystem_module_loader.cpp
  src/loading/module_path_resolver.cpp
  PUBLIC_DEPENDENCIES kyna_parsing kyna_diagnostics)
