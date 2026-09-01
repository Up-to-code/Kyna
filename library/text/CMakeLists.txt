kyna_add_module(kyna_text_library SOURCES
  src/types/unicode_text_error.cpp
  src/coordinates/code_point_offsets.cpp
  src/coordinates/unicode_space.cpp
  src/operations/length.cpp
  src/operations/find.cpp
  src/operations/slice.cpp
  src/operations/replace.cpp
  src/operations/split.cpp
  src/operations/trim.cpp
  src/operations/case_map.cpp
  PUBLIC_DEPENDENCIES utf8proc::utf8proc)
