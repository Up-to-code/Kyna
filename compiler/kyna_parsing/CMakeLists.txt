kyna_add_module(kyna_parsing SOURCES
  src/parsers/parser_core.cpp
  src/parsers/declaration_parser.cpp
  src/parsers/type_parser.cpp
  src/parsers/statement_parser.cpp
  src/parsers/expression_parser.cpp
  src/parsers/kyna_module_parser.cpp
  PUBLIC_DEPENDENCIES kyna_syntax kyna_lexing kyna_diagnostics)
