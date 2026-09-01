kyna_add_module(kyna_formats_library
  SOURCES
    src/formats/toml/toml_parse.cpp
    src/formats/toml/toml_stringify.cpp
    src/formats/xml/xml_parse.cpp
    src/formats/xml/xml_stringify.cpp
  PUBLIC_DEPENDENCIES tomlplusplus::tomlplusplus pugixml::pugixml)
