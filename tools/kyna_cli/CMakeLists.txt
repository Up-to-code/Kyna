add_executable(kyna
  src/kyna_main.cpp
  src/cli_argument_parser.cpp
  src/command_dispatcher.cpp
  src/source_run_command.cpp
  src/source_check_command.cpp
  src/repl_command.cpp
  src/token_dump_command.cpp
  src/syntax_dump_command.cpp
  src/hir_dump_command.cpp
  src/mir_dump_command.cpp
  src/bytecode_dump_command.cpp
  src/source_inspect_command.cpp
  src/project_commands.cpp
  src/kyna_formatter.cpp
  src/rich_diagnostic_renderer.cpp)
target_link_libraries(kyna PRIVATE kyna_embedding CLI11::CLI11 tomlplusplus::tomlplusplus
  ftxui::component ftxui::dom ftxui::screen)
kyna_configure_target(kyna)
set_target_properties(kyna PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(kyna PROPERTIES OUTPUT_NAME ky)
add_custom_command(TARGET kyna POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:kyna>"
          "$<TARGET_FILE_DIR:kyna>/kyna$<TARGET_FILE_SUFFIX:kyna>"
  COMMENT "Creating the supported kyna compatibility alias")
