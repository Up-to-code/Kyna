#include "../cli_commands.hpp"
#include "kyna/diagnostics/diagnostic_renderer.hpp"

namespace kyna::cli {

int dumpMir(const Options &options, LanguageSession &session, std::istream &input,
            std::ostream &output, std::ostream &errors) {
  std::string readError;
  auto source = readInput(options.input, input, readError);
  if (!readError.empty()) {
    errors << "kyna: " << readError << '\n';
    return 2;
  }
  auto result = session.inspectMir(options.input == "-" ? "<stdin>" : options.input,
                                   std::move(source), options.jsonOutput);
  if (!result.output.empty())
    output << result.output << (result.output.ends_with('\n') ? "" : "\n");
  if (!result.diagnostics.empty())
    errors << (options.jsonDiagnostics
                   ? renderJsonDiagnostics(result.diagnostics, session.sourceManager())
                   : renderCompilerDiagnostics(result.diagnostics, session.sourceManager(),
                                               {options.color}));
  return result.ok() ? 0 : 1;
}

} // namespace kyna::cli
