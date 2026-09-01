#include "../cli_commands.hpp"

namespace kyna::cli {

int checkSourceFile(const Options &options, LanguageSession &session, std::istream &input,
                    std::ostream &, std::ostream &errors) {
  if (options.input != "-")
    return renderResult(session.check(options.input), options, session, errors);
  std::string readError;
  auto source = readInput(options.input, input, readError);
  if (!readError.empty()) {
    errors << "kyna: " << readError << '\n';
    return 2;
  }
  auto result = options.sourceName.empty()
                    ? session.checkSource("<stdin>", std::move(source))
                    : session.checkSourceAtPath(options.sourceName, std::move(source));
  return renderResult(result, options, session, errors);
}

} // namespace kyna::cli
