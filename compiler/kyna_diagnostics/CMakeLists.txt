kyna_add_module(kyna_diagnostics SOURCES
  src/sinks/diagnostic_sink.cpp
  src/renderers/compiler_text_renderer.cpp
  src/renderers/json_diagnostic_renderer.cpp
  PUBLIC_DEPENDENCIES kyna_source)
