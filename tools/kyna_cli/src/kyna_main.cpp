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
  const bool terminal = _isatty(_fileno(stderr));
#else
  const bool terminal = isatty(fileno(stderr));
#endif
  options.color = options.color && (terminal || options.forceColor);
  options.richTerminal = options.color && terminal;
  return kyna::cli::dispatch(options, std::cin, std::cout, std::cerr);
}
