#include "../cli_commands.hpp"
#include <iostream>
#if defined(_WIN32)
#include <io.h>
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

int main(int argc, char **argv) {
  auto options = kyna::cli::parseArguments(argc, argv);
  options.executable = std::filesystem::absolute(argv[0]);
#if defined(__APPLE__)
  char executablePath[4096]; uint32_t executableSize = sizeof(executablePath);
  if (_NSGetExecutablePath(executablePath, &executableSize) == 0) options.executable = std::filesystem::canonical(executablePath);
#elif defined(_WIN32)
  std::wstring executablePath(32768, L'\0');
  auto executableSize = GetModuleFileNameW(nullptr, executablePath.data(), DWORD(executablePath.size()));
  if (executableSize && executableSize < executablePath.size()) {
    executablePath.resize(executableSize);
    options.executable = std::filesystem::path(executablePath);
  }
#elif defined(__linux__)
  options.executable = std::filesystem::read_symlink("/proc/self/exe");
#endif
#if defined(_WIN32)
  const bool terminal = _isatty(_fileno(stdin)) && _isatty(_fileno(stderr));
#else
  const bool terminal = isatty(fileno(stdin)) && isatty(fileno(stderr));
#endif
  options.interactiveTerminal = terminal && !options.noInteractive;
#if defined(_WIN32)
  const bool consoleTerminal = _isatty(_fileno(stdout));
#else
  const bool consoleTerminal = isatty(fileno(stdout));
#endif
  options.consoleColor = options.color && (consoleTerminal || options.forceColor);
  options.color = options.color && (terminal || options.forceColor);
  options.richTerminal = options.color && terminal;
  if (options.noInteractive || options.quiet) {
    options.richTerminal = false;
    options.progress = false;
  }
  try {
    return kyna::cli::dispatch(options, std::cin, std::cout, std::cerr);
  } catch (const kyna::KynaError &error) {
    kyna::LanguageSession session;
    kyna::LanguageResult result;
    result.diagnostics.push_back(error.diagnostic);
    kyna::cli::renderResult(result, options, session, std::cerr);
    return 2;
  }
}
