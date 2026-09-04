#include <kyna/execution/runtime_capabilities.hpp>

#include <cassert>
#include <iostream>
#include <string>

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

int main() {
#if defined(_WIN32)
  std::cout << "TEST SKIPPED: POSIX spawn is not available on Windows\n";
  return 0;
#else
  test_spawn_echoes_output();
  test_spawn_metacharacters_not_injected();
  test_spawn_nonexistent_program();
  test_spawn_nonzero_exit();
  std::cout << "TEST PASSED: spawn security tests\n";
  return 0;
#endif
}
