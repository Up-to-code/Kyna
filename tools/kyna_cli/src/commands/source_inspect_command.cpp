#include "../cli_commands.hpp"
#include <algorithm>

namespace kyna::cli {

int inspectSourceBytes(const Options &options, std::istream &input, std::ostream &output,
                       std::ostream &errors) {
  std::string readError;
  const auto source = readInput(options.input, input, readError);
  if (!readError.empty()) {
    errors << "kyna: " << readError << '\n';
    return 2;
  }

  const bool bom = source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
                   static_cast<unsigned char>(source[1]) == 0xBB &&
                   static_cast<unsigned char>(source[2]) == 0xBF;
  const auto nulBytes = static_cast<std::size_t>(std::count(source.begin(), source.end(), '\0'));
  std::size_t crlf = 0;
  for (std::size_t index = 1; index < source.size(); ++index)
    if (source[index - 1] == '\r' && source[index] == '\n')
      ++crlf;
  const auto lines = source.empty()
                         ? std::size_t{0}
                         : static_cast<std::size_t>(
                               std::count(source.begin(), source.end(), '\n')) +
                               (source.back() == '\n' ? 0 : 1);
  const bool trailingNewline = !source.empty() && source.back() == '\n';
  const bool suspicious = bom || nulBytes > 0;

  if (options.jsonOutput) {
    output << "{\"schema\":\"kyna.source-inspection/v1\",\"bytes\":" << source.size()
           << ",\"lines\":" << lines << ",\"utf8Bom\":" << (bom ? "true" : "false")
           << ",\"nulBytes\":" << nulBytes << ",\"crlf\":" << crlf
           << ",\"trailingNewline\":" << (trailingNewline ? "true" : "false")
           << ",\"suspicious\":" << (suspicious ? "true" : "false") << "}\n";
  } else {
    output << "Kyna source inspection\n"
           << "  bytes: " << source.size() << '\n'
           << "  lines: " << lines << '\n'
           << "  UTF-8 BOM: " << (bom ? "present" : "none") << '\n'
           << "  NUL bytes: " << nulBytes << '\n'
           << "  CRLF endings: " << crlf << '\n'
           << "  trailing newline: " << (trailingNewline ? "yes" : "no") << '\n'
           << "  status: " << (suspicious ? "suspicious bytes found" : "clean") << '\n';
  }
  return suspicious ? 1 : 0;
}

} // namespace kyna::cli
