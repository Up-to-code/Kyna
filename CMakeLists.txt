cmake_minimum_required(VERSION 3.25)
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
endif()

include(GNUInstallDirs)
install(TARGETS kyna RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE DESTINATION share/kyna)
install(DIRECTORY docs DESTINATION share/kyna)
install(DIRECTORY examples DESTINATION share/kyna FILES_MATCHING PATTERN "*.kyna")

set(CPACK_PACKAGE_NAME "kyna")
set(CPACK_PACKAGE_VENDOR "Up-to-code")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Kyna programming language and CLI")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Kyna")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_PACKAGE_FILE_NAME "kyna-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
include(CPack)
