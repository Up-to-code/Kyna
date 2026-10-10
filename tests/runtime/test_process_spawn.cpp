#include <kyna/execution/runtime_capabilities.hpp>

#include <cassert>
#include <iostream>
#include <string>
#include <filesystem>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <vector>
#endif

namespace {

#if !defined(_WIN32)
using namespace kyna;

void test_spawn_echoes_output() {
  ProcessConfig config;
  config.program = "/bin/echo";
  config.args = {"hello"};
  config.captureOutput = true;
  auto result = productionRuntimeCapabilities().processes->spawn(config);
  assert(!result.failedToStart);
  assert(result.exitCode == 0);
  assert(result.stdoutText.find("hello") != std::string::npos);
}

void test_spawn_metacharacters_not_injected() {
  // An attacker-controlled argument containing shell metacharacters should be
  // passed literally to echo, not interpreted as a command pipeline.
  ProcessConfig config;
  config.program = "/bin/echo";
  config.args = {"$(whoami)", "; rm -rf /"};
  config.captureOutput = true;
  auto result = productionRuntimeCapabilities().processes->spawn(config);
  assert(!result.failedToStart);
  assert(result.exitCode == 0);
  // echo should emit the metacharacters verbatim, not execute them.
  assert(result.stdoutText.find("$(whoami)") != std::string::npos);
  assert(result.stderrText.empty());
}

void test_spawn_nonexistent_program() {
  ProcessConfig config;
  config.program = "/usr/bin/this_program_definitely_does_not_exist_kyna_test";
  config.args = {};
  auto result = productionRuntimeCapabilities().processes->spawn(config);
  assert(result.failedToStart);
  assert(!result.startError.empty());
}

void test_spawn_nonzero_exit() {
  ProcessConfig config;
  config.program = "/bin/sh";
  config.args = {"-c", "exit 42"};
  config.captureOutput = true;
  auto result = productionRuntimeCapabilities().processes->spawn(config);
  assert(!result.failedToStart);
  assert(result.exitCode == 42);
}
#endif

} // namespace

int runTests(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "--echo-args") {
    for (int index = 2; index < argc; ++index)
      std::cout << std::string(argv[index]).size() << ":" << argv[index] << "\n";
    std::cerr << "captured stderr";
    return 23;
  }
  kyna::ProcessConfig config;
  config.program = std::filesystem::absolute(argv[0]).string();
  config.args = {"--echo-args", "", "a b", "quoted\"value", "trailing\\", "$(whoami)", "Unicode: \xCE\xBB"};
  config.captureOutput = true;
  auto result = kyna::productionRuntimeCapabilities().processes->spawn(config);
  assert(!result.failedToStart);
  assert(result.exitCode == 23);
  std::string expected;
  for (std::size_t index = 1; index < config.args.size(); ++index)
    expected += std::to_string(config.args[index].size()) + ":" + config.args[index] + "\n";
#if defined(_WIN32)
  std::string normalized;
  for (char c : result.stdoutText) if (c != '\r') normalized += c;
  assert(normalized == expected);
#else
  assert(result.stdoutText == expected);
#endif
  assert(result.stderrText == "captured stderr");
#if defined(_WIN32)
  std::cout << "TEST PASSED: Windows argv and stream capture\n";
  return 0;
#else
  config.program = "echo";
  config.args = {"PATH lookup"};
  result = kyna::productionRuntimeCapabilities().processes->spawn(config);
  assert(!result.failedToStart && result.stdoutText == "PATH lookup\n");
  test_spawn_echoes_output();
  test_spawn_metacharacters_not_injected();
  test_spawn_nonexistent_program();
  test_spawn_nonzero_exit();
  std::cout << "TEST PASSED: spawn security tests\n";
  return 0;
#endif
}

#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
  std::vector<std::string> storage;
  std::vector<char *> arguments;
  storage.reserve(argc);
  for (int index = 0; index < argc; ++index) {
    auto size = WideCharToMultiByte(CP_UTF8, 0, argv[index], -1, nullptr, 0, nullptr, nullptr);
    std::string text(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, argv[index], -1, text.data(), size, nullptr, nullptr);
    text.resize(size - 1);
    storage.push_back(std::move(text));
  }
  for (auto &text : storage) arguments.push_back(text.data());
  return runTests(argc, arguments.data());
}
#else
int main(int argc, char **argv) { return runTests(argc, argv); }
#endif
