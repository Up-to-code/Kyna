kyna_add_module(kyna_lexing SOURCES
  src/checkers/legacy_tokenizer_adapter.cpp
  src/checkers/token_scanner.cpp
  src/checkers/literal_scanner.cpp
  src/checkers/punctuation_scanner.cpp
  src/validators/keyword_catalog.cpp
  src/types/token_kind_description.cpp
  PUBLIC_DEPENDENCIES kyna_source kyna_diagnostics)
