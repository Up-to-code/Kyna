add_executable(kyna
  src/entry/kyna_main.cpp
  src/parser/cli_argument_parser.cpp
  src/dispatcher/command_dispatcher.cpp
  src/commands/source_run_command.cpp
  src/commands/source_check_command.cpp
  src/commands/source_inspect_command.cpp
  src/commands/token_dump_command.cpp
  src/commands/syntax_dump_command.cpp
  src/commands/hir_dump_command.cpp
  src/commands/mir_dump_command.cpp
  src/commands/bytecode_dump_command.cpp
  src/commands/repl/project.cpp
  src/commands/repl/editor.cpp
  src/commands/repl/run.cpp
  src/commands/project_dispatcher.cpp
  src/commands/project_discovery.cpp
  src/commands/project_fs.cpp
  src/commands/project_new.cpp
  src/commands/project_generate.cpp
  src/commands/project_format.cpp
  src/commands/project_dependencies.cpp
  src/commands/project_doctor.cpp
  src/commands/project_serve.cpp
  src/commands/project_dev.cpp
  src/commands/project_self.cpp
  src/formatting/kyna_formatter.cpp
  src/rendering/rich_diagnostic_renderer.cpp)
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
