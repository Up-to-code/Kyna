cmake_minimum_required(VERSION 3.25)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
project(kyna VERSION 1.0.0 LANGUAGES CXX)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KYNA_BUILD_TESTS "Build Kyna tests" ON)
option(KYNA_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(KYNA_FETCH_DEPENDENCIES "Fetch missing pinned development dependencies" ON)

find_package(CLI11 CONFIG QUIET)
find_package(CURL REQUIRED)
find_package(utf8proc 2.9 CONFIG QUIET)
if(APPLE AND NOT PostgreSQL_ROOT)
  foreach(KYNA_POSTGRESQL_HINT "/opt/homebrew/opt/libpq" "/usr/local/opt/libpq")
    if(EXISTS "${KYNA_POSTGRESQL_HINT}/include/libpq-fe.h")
      set(PostgreSQL_ROOT "${KYNA_POSTGRESQL_HINT}")
      break()
    endif()
  endforeach()
endif()
find_package(PostgreSQL QUIET)
if(NOT CLI11_FOUND AND KYNA_FETCH_DEPENDENCIES)
  include(FetchContent)
  FetchContent_Declare(CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.7.2
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(CLI11)
endif()
if(NOT TARGET CLI11::CLI11)
  message(FATAL_ERROR "CLI11 is required. Use vcpkg or configure with dependency fetching enabled.")
endif()

find_package(ftxui CONFIG QUIET)
if(NOT TARGET ftxui::screen AND KYNA_FETCH_DEPENDENCIES)
  include(FetchContent)
  set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v7.0.3
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(ftxui)
endif()
if(NOT TARGET ftxui::screen)
  message(FATAL_ERROR "FTXUI is required. Use vcpkg or configure with dependency fetching enabled.")
endif()

find_package(tomlplusplus CONFIG QUIET)
if(NOT TARGET tomlplusplus::tomlplusplus AND KYNA_FETCH_DEPENDENCIES)
  include(FetchContent)
  set(TOMLPLUSPLUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(TOMLPLUSPLUS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(tomlplusplus)
endif()
if(NOT TARGET tomlplusplus::tomlplusplus)
  message(FATAL_ERROR "toml++ is required. Use vcpkg or configure with dependency fetching enabled.")
endif()

find_package(pugixml CONFIG QUIET)
if(NOT TARGET pugixml::pugixml AND KYNA_FETCH_DEPENDENCIES)
  include(FetchContent)
  set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(PUGIXML_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
  set(PUGIXML_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG v1.15
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(pugixml)
endif()
if(TARGET pugixml AND NOT TARGET pugixml::pugixml)
  add_library(pugixml::pugixml ALIAS pugixml)
endif()
if(NOT TARGET pugixml::pugixml)
  message(FATAL_ERROR "pugixml is required. Use vcpkg or configure with dependency fetching enabled.")
endif()

if(NOT TARGET utf8proc::utf8proc AND KYNA_FETCH_DEPENDENCIES)
  include(FetchContent)
  set(UTF8PROC_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(utf8proc
    GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
    GIT_TAG v2.11.3
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(utf8proc)
endif()
if(TARGET utf8proc AND NOT TARGET utf8proc::utf8proc)
  add_library(utf8proc::utf8proc ALIAS utf8proc)
endif()
if(NOT TARGET utf8proc::utf8proc)
  message(FATAL_ERROR "utf8proc is required. Use vcpkg or configure with dependency fetching enabled.")
endif()

find_package(Boost 1.83 QUIET)
if(NOT Boost_FOUND AND KYNA_FETCH_DEPENDENCIES)
  include(FetchContent)
  set(BOOST_INCLUDE_LIBRARIES asio beast)
  FetchContent_Declare(Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.xz
    URL_HASH SHA256=7da75f171837577a52bbf217e17f8ea576c7c246e4594d617bfde7fafd408be5)
  FetchContent_MakeAvailable(Boost)
endif()
if(NOT Boost_FOUND AND NOT TARGET Boost::headers)
  message(FATAL_ERROR "Boost.Asio and Boost.Beast are required for the Kyna HTTP server.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(KynaModule)

# The subdirectory order documents the permitted dependency direction.
add_subdirectory(compiler/kyna_source)
add_subdirectory(compiler/kyna_diagnostics)
add_subdirectory(compiler/kyna_types)
add_subdirectory(compiler/kyna_lexing)
add_subdirectory(compiler/kyna_syntax)
add_subdirectory(compiler/kyna_parsing)
add_subdirectory(compiler/kyna_symbols)
add_subdirectory(compiler/kyna_resolution)
add_subdirectory(compiler/kyna_typecheck)
add_subdirectory(compiler/kyna_hir)
add_subdirectory(compiler/kyna_mir)
add_subdirectory(compiler/kyna_bytecode)
add_subdirectory(runtime/kyna_host)
add_subdirectory(runtime/kyna_vm)
add_subdirectory(library/collections)
add_subdirectory(library/database)
add_subdirectory(library/formats)
add_subdirectory(library/text)
add_subdirectory(library/core)
add_subdirectory(sdk/kyna_embedding)
add_subdirectory(tools/kyna_cli)

if(KYNA_BUILD_TESTS)
  enable_testing()
  add_test(NAME kyna_repository_architecture
           COMMAND Python3::Interpreter
                   "${CMAKE_CURRENT_SOURCE_DIR}/build_tools/verify_repository_architecture.py")
  add_test(NAME kyna_vscode_extension_manifest
           COMMAND Python3::Interpreter
                   "${CMAKE_CURRENT_SOURCE_DIR}/build_tools/verify_vscode_extension.py")
  function(add_kyna_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE kyna_embedding)
    if(MSVC)
      target_compile_options(${name} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${name} PRIVATE -UNDEBUG)
    endif()
    kyna_configure_target(${name})
    add_test(NAME ${name} COMMAND ${name})
  endfunction()
  add_kyna_test(kyna_tests tests/test_kyna.cpp)
  add_kyna_test(kyna_lexer_tests tests/lexer/test_lexer.cpp)
  add_kyna_test(kyna_parser_tests tests/parser/test_parser.cpp)
  add_kyna_test(kyna_semantic_tests tests/semantic/test_semantic.cpp)
  add_kyna_test(kyna_runtime_tests tests/runtime/test_runtime.cpp)
  add_kyna_test(kyna_stdlib_tests tests/stdlib/test_stdlib.cpp)
  add_kyna_test(kyna_language_session_tests tests/language_session/test_language_session.cpp)
  add_kyna_test(kyna_hir_tests tests/hir/test_hir.cpp)
  add_kyna_test(kyna_mir_tests tests/mir/test_mir.cpp)
  add_kyna_test(kyna_bytecode_tests tests/bytecode/test_bytecode.cpp)
  add_executable(kyna_formats_tests tests/formats/test_document_formats.cpp)
  target_link_libraries(kyna_formats_tests PRIVATE kyna_formats_library)
  if(MSVC)
    target_compile_options(kyna_formats_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(kyna_formats_tests PRIVATE -UNDEBUG)
  endif()
  kyna_configure_target(kyna_formats_tests)
  add_test(NAME kyna_formats_tests COMMAND kyna_formats_tests)
  add_executable(kyna_types_tests tests/types/test_type_hierarchy.cpp)
  target_link_libraries(kyna_types_tests PRIVATE kyna_types)
  if(MSVC)
    target_compile_options(kyna_types_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(kyna_types_tests PRIVATE -UNDEBUG)
  endif()
  kyna_configure_target(kyna_types_tests)
  add_test(NAME kyna_types_tests COMMAND kyna_types_tests)
  add_executable(kyna_syntax_tests tests/syntax/test_ast_visitor.cpp)
  target_link_libraries(kyna_syntax_tests PRIVATE kyna_syntax)
  if(MSVC)
    target_compile_options(kyna_syntax_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(kyna_syntax_tests PRIVATE -UNDEBUG)
  endif()
  kyna_configure_target(kyna_syntax_tests)
  add_test(NAME kyna_syntax_tests COMMAND kyna_syntax_tests)
  add_executable(kyna_scope_tests tests/semantics/test_scope_and_symbols.cpp)
  target_link_libraries(kyna_scope_tests PRIVATE kyna_typecheck)
  if(MSVC)
    target_compile_options(kyna_scope_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(kyna_scope_tests PRIVATE -UNDEBUG)
  endif()
  kyna_configure_target(kyna_scope_tests)
  add_test(NAME kyna_scope_tests COMMAND kyna_scope_tests)
  add_executable(kyna_export_cache_tests tests/semantics/test_export_cache.cpp)
  target_link_libraries(kyna_export_cache_tests PRIVATE kyna_typecheck kyna_resolution kyna_source)
  if(MSVC)
    target_compile_options(kyna_export_cache_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(kyna_export_cache_tests PRIVATE -UNDEBUG)
  endif()
  kyna_configure_target(kyna_export_cache_tests)
  add_test(NAME kyna_export_cache_tests COMMAND kyna_export_cache_tests)
  add_executable(kyna_package_tests tests/resolution/test_package_loader.cpp)
  target_link_libraries(kyna_package_tests PRIVATE kyna_resolution)
  if(MSVC)
    target_compile_options(kyna_package_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(kyna_package_tests PRIVATE -UNDEBUG)
  endif()
  kyna_configure_target(kyna_package_tests)
  add_test(NAME kyna_package_tests COMMAND kyna_package_tests)
  add_executable(kyna_streaming_tests tests/runtime/test_streaming.cpp)
  target_link_libraries(kyna_streaming_tests PRIVATE kyna_host)
  if(MSVC)
    target_compile_options(kyna_streaming_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(kyna_streaming_tests PRIVATE -UNDEBUG)
  endif()
  kyna_configure_target(kyna_streaming_tests)
  add_test(NAME kyna_streaming_tests COMMAND kyna_streaming_tests)
  add_executable(kyna_spawn_tests tests/runtime/test_process_spawn.cpp)
  target_link_libraries(kyna_spawn_tests PRIVATE kyna_host)
  if(MSVC)
    target_compile_options(kyna_spawn_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(kyna_spawn_tests PRIVATE -UNDEBUG)
  endif()
  kyna_configure_target(kyna_spawn_tests)
  add_test(NAME kyna_spawn_tests COMMAND kyna_spawn_tests)
  add_test(NAME kyna_cli_options_after_input
           COMMAND kyna run "${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.kyna" --no-color)
  add_test(NAME kyna_cli_file_alias
           COMMAND kyna "${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.kyna" --no-color)
  add_test(NAME kyna_cli_bytecode
           COMMAND kyna bytecode "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arithmetic.kyna"
                   --no-color)
  add_test(NAME kyna_cli_hir
           COMMAND kyna hir "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arithmetic.kyna"
                   --no-color)
  add_test(NAME kyna_cli_mir
           COMMAND kyna mir "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arithmetic.kyna"
                   --no-color)
  add_test(NAME kyna_vm_recursive_functions
           COMMAND kyna run
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/recursive_functions.kyna"
                   --no-color)
  set_tests_properties(kyna_vm_recursive_functions PROPERTIES
    PASS_REGULAR_EXPRESSION "factorial\\(6\\) 720.*fibonacci\\(10\\) 55")
  add_test(NAME kyna_cli_recursive_bytecode
           COMMAND kyna bytecode
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/recursive_functions.kyna"
                   --no-color)
  add_test(NAME kyna_vm_advanced_control_flow
           COMMAND kyna run
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/advanced_control_flow.kyna"
                   --no-color)
  set_tests_properties(kyna_vm_advanced_control_flow PROPERTIES
    PASS_REGULAR_EXPRESSION "visits 12.*shortCircuit false true")
  add_test(NAME kyna_cli_advanced_control_flow_mir
           COMMAND kyna mir
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/advanced_control_flow.kyna"
                   --no-color)
  add_test(NAME kyna_vm_first_class_functions
           COMMAND kyna run
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/first_class_functions.kyna"
                   --no-color)
  set_tests_properties(kyna_vm_first_class_functions PROPERTIES
    PASS_REGULAR_EXPRESSION "sum 42.*product 42")
  add_test(NAME kyna_cli_first_class_bytecode
           COMMAND kyna bytecode
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/first_class_functions.kyna"
                   --no-color)
  add_test(NAME kyna_vm_closures
           COMMAND kyna run
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/closures.kyna"
                   --no-color)
  set_tests_properties(kyna_vm_closures PROPERTIES
    PASS_REGULAR_EXPRESSION
      "addForty 42.*firstCounter 1 2.*secondCounter 11.*factorial 120.*nestedAdder 42")
  add_test(NAME kyna_cli_closure_bytecode
           COMMAND kyna bytecode
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/closures.kyna"
                   --no-color)
  add_test(NAME kyna_vm_exceptions
           COMMAND kyna run
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/exceptions.kyna"
                   --no-color)
  set_tests_properties(kyna_vm_exceptions PROPERTIES
    PASS_REGULAR_EXPRESSION "recovered returned .*cleanup-first.*cleanup-second.*return-cleanup")
  add_test(NAME kyna_cli_exception_bytecode
           COMMAND kyna bytecode
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/language/exceptions.kyna"
                   --no-color)
  add_test(NAME kyna_cli_color_policy
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_cli_color.py"
                   "$<TARGET_FILE:kyna>")
  add_test(NAME kyna_language_examples
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_language_examples.py"
                   "$<TARGET_FILE:kyna>"
                   "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME kyna_runtime_failures
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_runtime_failures.py"
                   "$<TARGET_FILE:kyna>"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime/failures")
  add_test(NAME kyna_network_examples
           COMMAND "${Python3_EXECUTABLE}"
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_network_examples.py"
                   "$<TARGET_FILE:kyna>"
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/network")
  add_test(NAME kyna_source_inspector
           COMMAND Python3::Interpreter
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_source_inspector.py"
                   "$<TARGET_FILE:kyna>"
                   "${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.kyna")
  add_test(NAME kyna_cli_heap_stats
           COMMAND kyna run "${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.kyna"
                   --heap-stats --no-color)
  set_tests_properties(kyna_cli_heap_stats PROPERTIES
    PASS_REGULAR_EXPRESSION "heap: live=.*objects=.*arrays=.*captures=.*closures=.*bound-methods=.*errors=")
  add_test(NAME kyna_developer_platform
           COMMAND Python3::Interpreter
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_developer_platform.py"
                   "$<TARGET_FILE:kyna>")
  set_tests_properties(kyna_developer_platform PROPERTIES TIMEOUT 90)
  add_test(NAME kyna_installed_cli
           COMMAND Python3::Interpreter
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_installed_cli.py"
                   "$<TARGET_FILE:kyna>"
                   "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(kyna_installed_cli PROPERTIES TIMEOUT 180 LABELS "integration;installer")
  add_test(NAME kyna_repl_terminal
           COMMAND Python3::Interpreter
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_repl_terminal.py"
                   "$<TARGET_FILE:kyna>")
  set_tests_properties(kyna_repl_terminal PROPERTIES TIMEOUT 30 LABELS "integration;terminal")
  add_test(NAME kyna_new_project_terminal
           COMMAND Python3::Interpreter
                   "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooling/verify_new_project_terminal.py"
                   "$<TARGET_FILE:kyna>")
  set_tests_properties(kyna_new_project_terminal PROPERTIES TIMEOUT 30 LABELS "integration;terminal")
endif()

include(GNUInstallDirs)
install(TARGETS kyna RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS "$<TARGET_FILE_DIR:kyna>/kyna$<TARGET_FILE_SUFFIX:kyna>"
        DESTINATION ${CMAKE_INSTALL_BINDIR})
if(WIN32)
  # vcpkg's app-local deployment places required DLLs beside ky.exe. Preserve
  # those dependencies in the staged archive so it works on a clean machine.
  install(DIRECTORY "$<TARGET_FILE_DIR:kyna>/"
          DESTINATION ${CMAKE_INSTALL_BINDIR}
          FILES_MATCHING PATTERN "*.dll")
endif()
install(FILES README.md LICENSE DESTINATION share/kyna)
install(DIRECTORY docs DESTINATION share/kyna)
install(DIRECTORY examples DESTINATION share/kyna FILES_MATCHING PATTERN "*.kyna")
install(DIRECTORY templates DESTINATION share/kyna OPTIONAL)

set(CPACK_PACKAGE_NAME "kyna")
set(CPACK_PACKAGE_VENDOR "Up-to-code")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Kyna programming language and CLI")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Kyna")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_PACKAGE_FILE_NAME "kyna-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
include(CPack)
