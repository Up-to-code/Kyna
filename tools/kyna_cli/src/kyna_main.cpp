#include "cli_commands.hpp"
#include <iostream>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

int main(int argc, char **argv) {
  auto options = kyna::cli::parseArguments(argc, argv);
#if defined(_WIN32)
  const bool terminal = _isatty(_fileno(stdin)) && _isatty(_fileno(stderr));
#else
  const bool terminal = isatty(fileno(stdin)) && isatty(fileno(stderr));
#endif
  options.interactiveTerminal = terminal && !options.noInteractive;
  options.color = options.color && (terminal || options.forceColor);
  options.richTerminal = options.color && terminal;
  if (options.noInteractive || options.quiet) {
    options.richTerminal = false;
    options.progress = false;
  }
  return kyna::cli::dispatch(options, std::cin, std::cout, std::cerr);
}
